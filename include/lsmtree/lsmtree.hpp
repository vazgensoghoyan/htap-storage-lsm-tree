#pragma once // lsmtree/lsmtree.hpp

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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
        const storage::StorageConfig& config
    );

    ~LSMTree();

    LSMTree(const LSMTree&) = delete;
    LSMTree& operator=(const LSMTree&) = delete;
    LSMTree(LSMTree&&) = delete;
    LSMTree& operator=(LSMTree&&) = delete;

    void insert(const storage::Row& row);

    std::unique_ptr<storage::ICursor> scan(
        const storage::read::sstable::KeyRange& range,
        const std::vector<std::size_t>& projection,
        storage::ScanOrder order
    ) const;

    const storage::Schema& schema() const noexcept;

    // Блокирует вызывающий поток, пока worker не разгребёт всю отложенную
    // работу: все immutable memtable сфлашены и compaction-политика больше
    // не возвращает задач. Используется для детерминированности (тесты,
    // явный «барьер» перед чтением гарантированного on-disk состояния).
    void wait_for_compaction();

private:
    // Точка входа worker-потока.
    void compaction_worker_loop();

    // Есть ли отложенная работа: непустая очередь immutable ИЛИ
    // политика хочет compaction. Вызывать под mutex_.
    bool has_pending_work_locked() const;

    // oldest immutable memtable -> L0 SSTable.
    // Возвращает true, если работа была проделана. Вызывается worker-ом
    // с захваченным lock; на время записи на диск lock временно отпускается.
    bool flush_one_locked(std::unique_lock<std::mutex>& lock);

    // Выполнить один шаг compaction, если политика его хочет.
    // Возвращает true, если compaction был выполнен. Семантика lock — как
    // у flush_one_locked.
    bool compact_one_locked(std::unique_lock<std::mutex>& lock);

    void save_manifest_locked() const;

    std::string build_sst_path(uint64_t sst_id) const;

    void delete_sst_files(uint64_t sst_id) const;

private:
    storage::Schema schema_;
    storage::StorageConfig config_;

    MemoryLayer memory_layer_;
    sstable::SSTableRegistry registry_;
    CompactionPolicy compaction_policy_;

    uint64_t next_sst_id_ = 0;

    mutable std::mutex mutex_;
    std::condition_variable worker_cv_; // будит worker (новая работа / стоп)
    std::condition_variable idle_cv_;   // будит ожидающих wait_for_compaction
    bool stop_ = false;                 // сигнал завершения
    bool worker_busy_ = false;          // worker сейчас выполняет шаг работы
    std::thread worker_;
};

} // namespace htap::lsmtree
