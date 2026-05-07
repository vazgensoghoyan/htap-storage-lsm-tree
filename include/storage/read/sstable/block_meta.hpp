#pragma once

#include "storage/api/types.hpp"

#include <cstddef>
#include <cstdint>

namespace htap::storage::read::sstable {

struct BlockMeta {
    Key min_key;
    Key max_key;

    std::uint64_t offset;
    std::uint64_t size;

    std::uint32_t row_count;
    std::size_t block_id;
};

}
