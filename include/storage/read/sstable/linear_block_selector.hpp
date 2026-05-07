#pragma once

#include "storage/read/sstable/block_meta.hpp"
#include "storage/read/sstable/key_range.hpp"

#include <optional>
#include <vector>

namespace htap::storage::read::sstable {

class LinearBlockSelector {
public:
    explicit LinearBlockSelector(std::vector<BlockMeta> blocks);

    const std::vector<BlockMeta>& blocks() const;

    std::vector<BlockMeta> select_blocks(const KeyRange& range) const;

    std::optional<BlockMeta> select_block_for_key(Key key) const;

private:
    static bool intersects(const BlockMeta& block, const KeyRange& range);

private:
    std::vector<BlockMeta> blocks_;
};

} 
