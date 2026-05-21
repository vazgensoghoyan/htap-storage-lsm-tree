#pragma once // lsmtree/sstable/metadata/sstable_registry.hpp

#include <vector>
#include <cstdint>

#include "lsmtree/sstable/metadata/sstable_info.hpp"

namespace htap::lsmtree {

class SSTableRegistry {
public:
    SSTableRegistry() = default;

    size_t level_count() const noexcept;
    size_t sstable_count(uint32_t level) const noexcept;

    void add(const SSTableInfo& info);
    void remove(uint64_t sst_id);

    const std::vector<SSTableInfo>& sstables_at_level(uint32_t level) const;

    std::vector<SSTableInfo> overlapping(uint32_t level, storage::OptKey from, storage::OptKey to) const;

private:
    std::vector<std::vector<SSTableInfo>> levels_;
};

} // namespace htap::lsmtree
