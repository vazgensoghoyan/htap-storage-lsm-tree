#include "storage/read/sstable/linear_row_block_selector.hpp"

#include <utility>

namespace htap::storage::read::sstable {

LinearRowBlockSelector::LinearRowBlockSelector(std::vector<RowBlockMeta> blocks)
    : blocks_(std::move(blocks)) {
}

const std::vector<RowBlockMeta>& LinearRowBlockSelector::blocks() const {
    return blocks_;
}

std::vector<RowBlockMeta> LinearRowBlockSelector::select_blocks(const KeyRange& range) const {
    std::vector<RowBlockMeta> selected;

    for (const auto& block : blocks_) {
        if (intersects(block, range)) {
            selected.push_back(block);
        }
    }

    return selected;
}

std::optional<RowBlockMeta> LinearRowBlockSelector::select_block_for_key(Key key) const {
    for (const auto& block : blocks_) {
        if (block.min_key <= key && key < block.max_key) {
            return block;
        }
    }

    return std::nullopt;
}

bool LinearRowBlockSelector::intersects(const RowBlockMeta& block, const KeyRange& range) {

    if (range.from.has_value() && block.max_key <= *range.from) {
        return false;
    }

    if (range.to.has_value() && block.min_key >= *range.to) {
        return false;
    }

    return true;
}

}
