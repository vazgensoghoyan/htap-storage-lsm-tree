#pragma once // lsmtree/sstable/format/sst_layout.hpp

#include <cstdint>

#include "storage/api/types.hpp"

namespace htap::lsmtree::sstable {

enum class SSTLayout : uint8_t {
    ROW = 0,
    COLUMN = 1
};

} // namespace htap::lsmtree::sstable
