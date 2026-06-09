#pragma once

#include "storage/api/types.hpp"

#include <cstddef>
#include <cstdint>

namespace htap::storage::read::sstable {

constexpr std::uint64_t ROW_BLOCK_META_ON_DISK_SIZE =
    sizeof(Key) +              // min_key
    sizeof(Key) +              // max_key
    sizeof(std::uint64_t) +    // offset
    sizeof(std::uint64_t) +    // size_bytes
    sizeof(std::uint32_t) +    // row_count
    sizeof(std::uint32_t);     // block_id

struct RowBlockMeta {
    Key min_key;
    Key max_key;

    std::uint64_t offset;
    std::uint64_t size;

    std::uint32_t row_count;
    std::size_t block_id;
};

}
