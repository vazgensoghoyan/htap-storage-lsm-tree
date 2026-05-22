#include "storage/read/sstable/sparse_block_selector.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace htap::storage::read::sstable {

namespace {

bool intersects(Key min_key, Key max_key, const KeyRange& range) {
    if (range.from && max_key <= *range.from) {
        return false;
    }

    if (range.to && min_key >= *range.to) {
        return false;
    }

    return true;
}

} 

MetadataBlockRange SparseBlockSelector::select_metadata_range(
    const std::vector<htap::lsmtree::sstable::SparseIndexEntry>& index,
    const KeyRange& range,
    std::uint32_t total_blocks
) const {
    
    if (total_blocks == 0) {
        return MetadataBlockRange{
            .first_block_id = 0,
            .block_count = 0
        };
    }

    if (index.empty()) {
        return MetadataBlockRange{
            .first_block_id = 0,
            .block_count = total_blocks
        };
    }

    std::uint32_t first_block_id = 0;

    if (range.from) {
        const auto upper = std::upper_bound(
            index.begin(),
            index.end(),
            *range.from,
            [](Key key, const htap::lsmtree::sstable::SparseIndexEntry& entry) {
                return key < entry.min_key;
            }
        );

        if (upper != index.begin()) {
            first_block_id = std::prev(upper)->block_id;
        }
    }

    if (first_block_id >= total_blocks) {
        throw std::runtime_error("Sparse index block_id is out of range");
    }

    std::uint32_t end_block_id = total_blocks;

    if (range.to) {
        const auto lower = std::lower_bound(
            index.begin(),
            index.end(),
            *range.to,
            [](const htap::lsmtree::sstable::SparseIndexEntry& entry, Key key) {
                return entry.min_key < key;
            }
        );

        if (lower != index.end()) {
            end_block_id = lower->block_id;

            if (end_block_id >= total_blocks) {
                throw std::runtime_error("Sparse index block_id is out of range");
            }
        }
    }

    if (end_block_id <= first_block_id) {
        return MetadataBlockRange{
            .first_block_id = first_block_id,
            .block_count = 0
        };
    }

    return MetadataBlockRange{
        .first_block_id = first_block_id,
        .block_count = end_block_id - first_block_id
    };
}

std::vector<RowBlockMeta> SparseBlockSelector::filter_candidate_row_blocks(
    const std::vector<RowBlockMeta>& candidates,
    const KeyRange& range
) const {
    std::vector<RowBlockMeta> selected;

    for (const auto& block : candidates) {
        if (intersects(block.min_key, block.max_key, range)) {
            selected.push_back(block);
        }
    }

    return selected;
}


std::vector<ColumnBlockMeta> SparseBlockSelector::filter_candidate_column_blocks(
    const std::vector<ColumnBlockMeta>& candidates,
    const KeyRange& range
) const {
    std::vector<ColumnBlockMeta> selected;

    for (const auto& block : candidates) {
        if (intersects(block.min_key, block.max_key, range)) {
            selected.push_back(block);
        }
    }

    return selected;
}


}
