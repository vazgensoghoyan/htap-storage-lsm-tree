#pragma once // lsmtree/sstable/format/sstable_info.hpp

#include <cstdint>
#include <cstddef>

#include "storage/api/types.hpp"

namespace htap::lsmtree::sstable::format {

enum class SSTLayout : uint8_t {
    ROW = 0,
    COLUMN = 1
};

static constexpr uint32_t SST_MAGIC = 0x53535431; // "SST1" в little-endian

struct SSTableInfo {
    uint32_t magic = SST_MAGIC; // "SST1" например

    uint32_t num_blocks;

    storage::Key min_key;
    storage::Key max_key;

    SSTLayout layout_type; // ROW or COLUMN
};

} // namespace htap::lsmtree::sstable::format
