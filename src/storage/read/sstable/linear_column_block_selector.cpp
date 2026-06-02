#include "storage/read/sstable/linear_column_block_selector.hpp"

#include <utility>

namespace htap::storage::read::sstable {

LinearColumnBlockSelector::LinearColumnBlockSelector(
    std::vector<ColumnBlockMeta> blocks
) : blocks_(std::move(blocks)){

}

std::vector<ColumnBlockMeta> LinearColumnBlockSelector::select_blocks(const KeyRange& range) const {
    std::vector<ColumnBlockMeta> result;

    for (const auto& block : blocks_) {
        if (intersects(block, range)) {
            result.push_back(block);
        }
    }

    return result;
}

std::vector<ColumnBlockMeta> LinearColumnBlockSelector::select_block_group_for_key(Key key) const {
    std::vector<ColumnBlockMeta> result;

    for (const auto& block : blocks_) {
        if (block.min_key <= key && key <= block.max_key) {
            result.push_back(block);
        }
    }

    return result;
}

bool LinearColumnBlockSelector::intersects(
    const ColumnBlockMeta& block,
    const KeyRange& range
) {
    if (range.from.has_value() && block.max_key < *range.from) {
        return false;
    }

    if (range.to.has_value() && block.min_key >= *range.to) {
        return false;
    }

    return true;
}

} 
