#pragma once

#include "storage/api/types.hpp"

#include <cstddef>
#include <cstdint>

namespace htap::storage::read::sstable {

constexpr std::uint64_t COLUMN_BLOCK_META_ON_DISK_SIZE =
    sizeof(Key) +              // min_key
    sizeof(Key) +              // max_key
    sizeof(std::uint64_t) +    // offset
    sizeof(std::uint64_t) +    // size_bytes
    sizeof(std::uint32_t) +    // values_count
    sizeof(std::uint32_t) +    // block_id
    sizeof(std::uint32_t);     // column_idx

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
