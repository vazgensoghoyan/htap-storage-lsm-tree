#pragma once // lsmtree/sstable/metadata/sstable_info.hpp

#include <string>
#include <cstdint>

#include "storage/api/types.hpp"

namespace htap::lsmtree {

enum class SSTLayout : uint8_t {
    ROW = 0,
    COLUMN = 1
};

struct SSTableInfo {
    uint64_t id;

    std::string path;

    uint32_t level;

    storage::Key min_key;
    storage::Key max_key;

    uint64_t file_size_bytes;

    uint32_t num_blocks;

    SSTLayout layout;
};

} // namespace htap::lsmtree
