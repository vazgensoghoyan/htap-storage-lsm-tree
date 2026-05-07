#include "storage/read/sstable/linear_block_selector.hpp"

#include <utility>

namespace htap::storage::read::sstable {

LinearBlockSelector::LinearBlockSelector(std::vector<BlockMeta> blocks)
    : blocks_(std::move(blocks)) {
}

const std::vector<BlockMeta>& LinearBlockSelector::blocks() const {
    return blocks_;
}

std::vector<BlockMeta> LinearBlockSelector::select_blocks(const KeyRange& range) const {
    std::vector<BlockMeta> selected;

    for (const auto& block : blocks_) {
        if (intersects(block, range)) {
            selected.push_back(block);
        }
    }

    return selected;
}

std::optional<BlockMeta> LinearBlockSelector::select_block_for_key(Key key) const {
    for (const auto& block : blocks_) {
        if (block.min_key <= key && key < block.max_key) {
            return block;
        }
    }

    return std::nullopt;
}

bool LinearBlockSelector::intersects(const BlockMeta& block, const KeyRange& range) {

    if (range.from.has_value() && block.max_key <= *range.from) {
        return false;
    }

    if (range.to.has_value() && block.min_key >= *range.to) {
        return false;
    }

    return true;
}

}
