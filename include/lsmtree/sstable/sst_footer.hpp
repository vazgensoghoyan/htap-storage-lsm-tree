#pragma once // lsmtree/sstable/sst_footer.hpp

#include <cstdint>
#include <cstddef>

namespace htap::lsmtree {

static constexpr uint8_t ROW_LAYOUT = 0;
static constexpr uint8_t COLUMN_LAYOUT = 1;

static constexpr uint32_t SST_MAGIC = 0x53535431; // "SST1" в little-endian

struct SSTFooter {
    uint32_t magic = SST_MAGIC; // "SST1" например

    uint32_t num_blocks;
    uint64_t meta_offset;

    uint64_t min_key;
    uint64_t max_key;

    uint8_t layout_type; // ROW or COLUMN
};

} // namespace htap::lsmtree
