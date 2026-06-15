#pragma once // lsmtree/lsmtree.hpp

#include "storage/model/schema.hpp"
#include "storage/api/types.hpp"
#include "storage/api/cursor_interface.hpp"
#include "storage/read/sstable/key_range.hpp"

#include "lsmtree/mem/memory_layer.hpp"
#include "lsmtree/sstable/metadata/sstable_registry.hpp"

#include <string>
#include <memory>
#include <vector>

namespace htap::lsmtree {

class LSMTree {
public:
    LSMTree(
        const storage::Schema& schema,
        const std::string& path,
        size_t memtable_threshold = DEFAULT_MEMTABLE_THRESHOLD
    );

    void insert(const storage::Row& row);

    std::unique_ptr<storage::ICursor> scan(
        const storage::read::sstable::KeyRange& range,
        const std::vector<std::size_t>& projection,
        storage::ScanOrder order
    ) const;

    const storage::Schema& schema() const noexcept;

private:
    void flush_memtable();
    std::string build_sst_path(uint64_t sst_id) const;

private:
    storage::Schema schema_;

    std::string path_;

    MemoryLayer memory_layer_;

    sstable::SSTableRegistry registry_;

    uint64_t next_sst_id_ = 0;
};

} // namespace htap::lsmtree
