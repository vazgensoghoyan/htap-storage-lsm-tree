#pragma once // lsmtree/compaction/compaction_policy.hpp

#include <optional>

#include "lsmtree/compaction/compaction_task.hpp"
#include "storage/api/config.hpp"
#include "lsmtree/sstable/metadata/sstable_registry.hpp"

namespace htap::lsmtree {

/**
 * Стратегия выбора compaction
 *
 * Текущая реализация (SimpleLeveledPolicy):
 *  - SST на L0 >= config.level0_compaction_trigger -> compact L0->L1 (COLUMN)
 *  - суммарный размер файлов на L_i > порог -> compact L_i->L_(i+1) (COLUMN)
 *
 * Возвращает std::nullopt если compaction не нужен
 */
class CompactionPolicy {
public:
    explicit CompactionPolicy(const StorageConfig& config);

    std::optional<CompactionTask> pick(const sstable::SSTableRegistry& registry) const;

private:
    const StorageConfig& config_;
};

} // namespace htap::lsmtree
