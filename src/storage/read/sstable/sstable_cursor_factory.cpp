#include "storage/read/sstable/sstable_cursor_factory.hpp"

#include "storage/cursor/sstable_column_cursor.hpp"
#include "storage/cursor/sstable_row_cursor.hpp"
#include "storage/read/sstable/sparse_block_selector.hpp"
#include "storage/read/sstable/sstable_reader.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

namespace htap::storage::read::sstable {

std::unique_ptr<ICursor> make_sstable_cursor(
    const lsmtree::sstable::SSTableInfo& info,
    const KeyRange& range,
    const std::vector<ValueType>& schema,
    const std::vector<std::size_t>& projection
) {
    std::unique_ptr<SSTableReader> reader = std::make_unique<SSTableReader>(info.path);
    SparseBlockSelector selector;

    const auto sparse_index = reader->read_sparse_index();

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
            auto candidates = reader->read_row_metadata_range(
                selected_metadata_range.first_block_id,
                selected_metadata_range.block_count
            );

            auto filtered_blocks = selector.filter_candidate_row_blocks(
                candidates,
                range
            );

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
            auto candidates = reader->read_column_metadata_range(
                selected_metadata_range.first_block_id,
                selected_metadata_range.block_count,
                static_cast<std::uint32_t>(schema.size())
            );

            auto filtered_blocks = selector.filter_candidate_column_blocks(
                candidates,
                range
            );

            if (filtered_blocks.empty()) {
                return nullptr;
            }

            return std::make_unique<cursor::SSTableColumnCursor>(
                std::move(reader),
                std::move(filtered_blocks),
                range,
                schema,
                projection
            );
        }

    }

    throw std::runtime_error("Unsupported SSTable layout type");
}

}