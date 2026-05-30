#pragma once // lsmtree/sstable/format/sparse_index_entry.hpp

#include <cstdint>

#include "storage/api/types.hpp"

namespace htap::lsmtree::format {

struct SparseIndexEntry {
    storage::Key min_key;
    uint32_t block_id;
};

} // namespace htap::lsmtree::format
