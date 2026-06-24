#pragma once // lsmtree/sstable/metadata/sstable_info.hpp

#include <string>
#include <cstdint>

#include "storage/api/types.hpp"
#include "lsmtree/sstable/format/sst_layout.hpp"

namespace htap::lsmtree::sstable {

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

} // namespace htap::lsmtree::sstable
