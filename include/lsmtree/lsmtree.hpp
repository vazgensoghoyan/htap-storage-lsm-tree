#pragma once // lsmtree/lsmtree.hpp

#include <memory>
#include <string>
#include <vector>

#include "storage/api/cursor_interface.hpp"
#include "storage/api/types.hpp"
#include "storage/model/schema.hpp"
#include "storage/read/sstable/key_range.hpp"

#include "storage/api/config.hpp"
#include "lsmtree/compaction/compaction_policy.hpp"
#include "lsmtree/mem/memory_layer.hpp"
#include "lsmtree/sstable/metadata/sstable_registry.hpp"

namespace htap::lsmtree {

class LSMTree {
public:
    explicit LSMTree(
        const storage::Schema& schema,
        const StorageConfig& config
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
    void maybe_compact();
    void save_manifest() const;

    std::string build_sst_path(uint64_t sst_id) const;

    void delete_sst_files(uint64_t sst_id) const;

private:
    storage::Schema schema_;
    StorageConfig config_;

    MemoryLayer memory_layer_;
    sstable::SSTableRegistry registry_;
    CompactionPolicy compaction_policy_;

    uint64_t next_sst_id_ = 0;
};

} // namespace htap::lsmtree
