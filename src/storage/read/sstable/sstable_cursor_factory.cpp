#include "storage/read/sstable/sstable_cursor_factory.hpp"

#include "storage/cursor/sstable_column_cursor.hpp"
#include "storage/cursor/sstable_row_cursor.hpp"
#include "storage/read/sstable/data_skipping_block_pruner.hpp"
#include "storage/read/sstable/sparse_block_selector.hpp"
#include "storage/read/sstable/sstable_reader.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace htap::storage::read::sstable {

std::unique_ptr<ICursor> make_sstable_cursor(
    const lsmtree::sstable::SSTableInfo& info,
    SSTableMetadataCache& metadata_cache,
    const KeyRange& range,
    const std::vector<ValueType>& schema,
    const std::vector<std::size_t>& projection,
    const read::DataSkippingFilter& data_skipping_filter
) {
    std::unique_ptr<SSTableReader> reader = std::make_unique<SSTableReader>(info.path);
    SparseBlockSelector selector;

    const auto& sparse_index = metadata_cache.sparse_index();

    const auto selected_metadata_range = selector.select_metadata_range(
        sparse_index,
        range,
        info.num_blocks
    );

    if (selected_metadata_range.block_count == 0) {
        return nullptr;
    }

    switch (info.layout) {
        case lsmtree::sstable::SSTLayout::ROW: {
            auto candidates = metadata_cache.read_row_metadata_range(
                selected_metadata_range.first_block_id,
                selected_metadata_range.block_count
            );

            auto filtered_blocks = selector.filter_candidate_row_blocks(
                candidates,
                range
            );

            if (!filtered_blocks.empty() && !data_skipping_filter.empty()) {
                const auto stats = metadata_cache.read_numeric_stats_range(
                    selected_metadata_range.first_block_id,
                    selected_metadata_range.block_count,
                    data_skipping_filter.referenced_columns()
                );

                DataSkippingBlockPruner pruner;
                filtered_blocks = pruner.filter_row_blocks(
                    filtered_blocks,
                    stats,
                    data_skipping_filter
                );
            }

            if (filtered_blocks.empty()) {
                return nullptr;
            }

            return std::make_unique<cursor::SSTableRowCursor>(
                std::move(reader),
                std::move(filtered_blocks),
                range,
                schema,
                projection
            );
        }
        
        case lsmtree::sstable::SSTLayout::COLUMN: {
            auto candidates = metadata_cache.read_column_metadata_range(
                selected_metadata_range.first_block_id,
                selected_metadata_range.block_count
            );

            auto filtered_blocks = selector.filter_candidate_column_blocks(
                candidates,
                range
            );

            if (!filtered_blocks.empty() && !data_skipping_filter.empty()) {
                std::vector<std::uint32_t> logical_block_ids;
                logical_block_ids.reserve(filtered_blocks.size());

                for (const auto& block : filtered_blocks) {
                    const auto block_id = static_cast<std::uint32_t>(block.block_id);
                    if (logical_block_ids.empty() || logical_block_ids.back() != block_id) {
                        logical_block_ids.push_back(block_id);
                    }
                }

                const auto stats = metadata_cache.read_numeric_stats_range(
                    selected_metadata_range.first_block_id,
                    selected_metadata_range.block_count,
                    data_skipping_filter.referenced_columns()
                );

                DataSkippingBlockPruner pruner;
                const auto selected_logical_block_ids = pruner.filter_logical_block_ids(
                    logical_block_ids,
                    stats,
                    data_skipping_filter
                );

                const std::unordered_set<std::uint32_t> selected_block_ids(
                    selected_logical_block_ids.begin(),
                    selected_logical_block_ids.end()
                );

                std::vector<ColumnBlockMeta> pruned_blocks;
                pruned_blocks.reserve(filtered_blocks.size());

                for (const auto& block : filtered_blocks) {
                    if (selected_block_ids.contains(static_cast<std::uint32_t>(block.block_id))) {
                        pruned_blocks.push_back(block);
                    }
                }

                filtered_blocks = std::move(pruned_blocks);
            }

            if (filtered_blocks.empty()) {
                return nullptr;
            }

            std::vector<ColumnBlockMeta> projected_blocks;
            projected_blocks.reserve(filtered_blocks.size());

            for (const auto& block : filtered_blocks) {
                if (block.column_idx == KEY_COLUMN_INDEX ||
                    std::find(projection.begin(), projection.end(), block.column_idx) != projection.end()) {
                    projected_blocks.push_back(block);
                }
            }

            return std::make_unique<cursor::SSTableColumnCursor>(
                std::move(reader),
                std::move(projected_blocks),
                range,
                schema,
                projection
            );
        }

    }

    throw std::runtime_error("Unsupported SSTable layout type");
}

}
