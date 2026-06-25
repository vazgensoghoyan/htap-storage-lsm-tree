#pragma once // lsmtree/compaction/compaction_policy.hpp

#include <cstdint>

#include "lsmtree/sstable/format/sst_layout.hpp"

namespace htap::lsmtree {

// Описывает одну задачу compaction: какой уровень компактируем, куда, с каким layout
struct CompactionTask {
    uint32_t src_level;
    uint32_t dst_level;
    sstable::SSTLayout output_layout;
};

} // namespace htap::lsmtree
