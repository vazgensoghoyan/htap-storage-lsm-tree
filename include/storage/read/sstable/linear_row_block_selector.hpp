#pragma once

#include "storage/read/sstable/row_block_meta.hpp"
#include "storage/read/sstable/key_range.hpp"

#include <optional>
#include <vector>

namespace htap::storage::read::sstable {

class LinearRowBlockSelector {
public:
    explicit LinearRowBlockSelector(std::vector<RowBlockMeta> blocks);

    const std::vector<RowBlockMeta>& blocks() const;

    std::vector<RowBlockMeta> select_blocks(const KeyRange& range) const;

    std::optional<RowBlockMeta> select_block_for_key(Key key) const;

private:
    static bool intersects(const RowBlockMeta& block, const KeyRange& range);

private:
    std::vector<RowBlockMeta> blocks_;
};

} 
