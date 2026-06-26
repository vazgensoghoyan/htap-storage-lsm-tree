#pragma once // lsmtree/compaction/compaction_job.hpp

#include <cstdint>
#include <string>
#include <vector>

#include "lsmtree/compaction/compaction_task.hpp"
#include "lsmtree/sstable/metadata/sstable_info.hpp"
#include "storage/api/config.hpp"
#include "storage/model/schema.hpp"

namespace htap::lsmtree {

/**
 * Выполняет один шаг compaction:
 *  1. Открывает cursor на каждый SST с src_level
 *  2. Мержит через MergeCursor (глобально отсортированный поток строк)
 *  3. Пишет новый SST на dst_level с нужным layout
 *  4. Возвращает SSTableInfo нового SST (НЕ трогает реестр)
 *
 * !!! Удаление старых файлов — ответственность вызывающего (LSMTree)
 */
class CompactionJob {
public:
    CompactionJob(
        const storage::Schema& schema,
        const std::string& table_path,
        const storage::StorageConfig& config
    );

    sstable::SSTableInfo run(
        const std::vector<sstable::SSTableInfo>& candidates,
        const CompactionTask& task,
        uint64_t new_sst_id
    );

private:
    std::string build_sst_path(uint64_t sst_id) const;

private:
    const storage::Schema& schema_;
    std::string table_path_;
    storage::StorageConfig config_;
};

} // namespace htap::lsmtree
