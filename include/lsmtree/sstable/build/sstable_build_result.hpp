#pragma once // lsmtree/sstable/build/sst_footer.hpp

#include <cstdint>
#include <cstddef>

#include "storage/api/types.hpp"

namespace htap::lsmtree::sstable {

struct SSTableBuildResult {
    storage::Key min_key;
    storage::Key max_key;
    uint32_t num_blocks;
};

} // namespace htap::lsmtree::sstable
