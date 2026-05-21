#pragma once

#include <cstdint>

#include "storage/api/types.hpp"

namespace htap::lsmtree::sstable {

struct SparseIndexEntry {
    storage::Key min_key;
    std::uint32_t block_id;
};

}