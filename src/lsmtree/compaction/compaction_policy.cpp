#include "lsmtree/compaction/compaction_policy.hpp"

#include <cstdint>
#include <optional>

using namespace htap::lsmtree;
using namespace htap::lsmtree::sstable;

CompactionPolicy::CompactionPolicy(const storage::StorageConfig& config)
    : config_(config)
{}

std::optional<CompactionTask> CompactionPolicy::pick(const SSTableRegistry& registry) const {
    // 1. Level-0 → Level-1: триггер по числу SST
    if (registry.sstable_count(0) >= config_.level0_compaction_trigger) {
        const SSTLayout output_layout =
            (config_.row_to_column_level <= 1)
                ? SSTLayout::COLUMN
                : SSTLayout::ROW;

        return CompactionTask{
            .src_level     = 0,
            .dst_level     = 1,
            .output_layout = output_layout
        };
    }

    // 2. Level-i → Level-(i+1): триггер по суммарному размеру файлов
    for (uint32_t level = 1; level < static_cast<uint32_t>(registry.level_count()); ++level) {
        const auto& sstables = registry.sstables_at_level(level);
        if (sstables.empty()) continue;

        // Порог размера level = base * size_ratio^level
        uint64_t threshold = config_.base_level_size_bytes;
        for (uint32_t i = 0; i < level; ++i) {
            threshold *= static_cast<uint64_t>(config_.size_ratio);
        }

        uint64_t total_size = 0;
        for (const auto& sst : sstables) {
            total_size += sst.file_size_bytes;
        }

        if (total_size > threshold) {
            const uint32_t dst_level = level + 1;
            const SSTLayout output_layout =
                (dst_level >= config_.row_to_column_level)
                    ? SSTLayout::COLUMN
                    : SSTLayout::ROW;

            return CompactionTask{
                .src_level     = level,
                .dst_level     = dst_level,
                .output_layout = output_layout
            };
        }
    }

    return std::nullopt;
}
