#include "lsmtree/sstable/metadata/sstable_registry.hpp"

#include <algorithm>
#include <stdexcept>

using namespace htap::storage;
using namespace htap::lsmtree;
using namespace htap::lsmtree::sstable;

size_t SSTableRegistry::level_count() const {
    return levels_.size();
}

size_t SSTableRegistry::sstable_count(uint32_t level) const {
    if (level >= levels_.size())
        return 0;

    return levels_[level].size();
}

void SSTableRegistry::add(const SSTableInfo& info) {
    if (levels_.size() <= info.level)
        levels_.resize(info.level + 1);

    levels_[info.level].push_back(info);
}

void SSTableRegistry::remove(uint64_t sst_id) {
    for (auto& level : levels_) {
        auto it = std::remove_if(
            level.begin(),
            level.end(),
            [sst_id](const SSTableInfo& info) { return info.id == sst_id; }
        );

        level.erase(it, level.end());
    }
}

const std::vector<SSTableInfo>& SSTableRegistry::sstables_at_level(uint32_t level) const {
    if (level >= levels_.size())
        throw std::out_of_range("Invalid LSM level");

    return levels_[level];
}

std::vector<SSTableInfo> SSTableRegistry::overlapping(uint32_t level, OptKey from, OptKey to) const {
    std::vector<SSTableInfo> result;

    if (level >= levels_.size())
        return result;

    for (const auto& sst : levels_[level]) {

        if (from.has_value() && sst.max_key < from.value())
            continue;

        if (to.has_value() && sst.min_key >= to.value())
            continue;

        result.push_back(sst);
    }

    return result;
}
