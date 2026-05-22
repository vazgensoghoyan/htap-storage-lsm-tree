#pragma once

#include <cstdint>

#include "storage/api/types.hpp"

namespace htap::lsmtree::sstable {

constexpr std::uint64_t SPARSE_INDEX_ENTRY_ON_DISK_SIZE = 
    sizeof(storage::Key) + sizeof(std::uint32_t);

struct SparseIndexEntry {
    storage::Key min_key;
    std::uint32_t block_id;
};

}