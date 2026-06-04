#pragma once

#include <cstdint>
#include <vector>

#include "lsmtree/sstable/sparse_index/sparse_index_entry.hpp"
#include "storage/read/sstable/key_range.hpp"
#include "storage/read/sstable/row_block_meta.hpp"
#include "storage/read/sstable/column_block_meta.hpp"

namespace htap::storage::read::sstable {

struct MetadataBlockRange {
    std::uint32_t first_block_id;
    std::uint32_t block_count;
};

class SparseBlockSelector {
public:
    MetadataBlockRange select_metadata_range(
        const std::vector<lsmtree::sstable::SparseIndexEntry>& index,
        const KeyRange& range,
        std::uint32_t total_blocks
    ) const;

    std::vector<RowBlockMeta> filter_candidate_row_blocks(
        const std::vector<RowBlockMeta>& candidates,
        const KeyRange& range
    ) const;

    std::vector<ColumnBlockMeta> filter_candidate_column_blocks(
        const std::vector<ColumnBlockMeta>& candidates,
        const KeyRange& range
    ) const;
};

} 
