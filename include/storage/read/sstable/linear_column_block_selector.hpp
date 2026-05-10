#pragma once

#include "storage/api/types.hpp"
#include "storage/read/sstable/column_block_meta.hpp"
#include "storage/read/sstable/key_range.hpp"

#include <vector>

namespace htap::storage::read::sstable {

class LinearColumnBlockSelector {
public:
    explicit LinearColumnBlockSelector(std::vector<ColumnBlockMeta> blocks);

    std::vector<ColumnBlockMeta> select_blocks(const KeyRange& range) const;

    std::vector<ColumnBlockMeta> select_block_group_for_key(Key key) const;

private:
    static bool intersects(const ColumnBlockMeta& block, const KeyRange& range);

    std::vector<ColumnBlockMeta> blocks_;
};

} 
