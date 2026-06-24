#pragma once // lsmtree/sstable/format/sparse_index_entry.hpp

#include <cstdint>

#include "storage/api/types.hpp"

namespace htap::lsmtree::sstable {

constexpr uint64_t SPARSE_INDEX_ENTRY_ON_DISK_SIZE = sizeof(storage::Key) + sizeof(uint32_t);

struct SparseIndexEntry {
    storage::Key min_key;
    uint32_t block_id;
};

} // namespace htap::lsmtree::sstable
