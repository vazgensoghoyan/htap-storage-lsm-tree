#pragma once

#include "storage/api/types.hpp"

#include <cstddef>
#include <cstdint>

namespace htap::storage::read::sstable {

struct ColumnBlockMeta {
    Key min_key;
    Key max_key;

    std::uint64_t offset;
    std::uint64_t size;

    std::uint32_t values_count;

    std::size_t block_id;
    std::size_t column_idx;
};

}
