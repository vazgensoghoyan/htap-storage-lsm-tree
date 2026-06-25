#include "lsmtree/lsmtree.hpp"

#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <utility>
#include <vector>

#include "utils/logger.hpp"

#include "lsmtree/compaction/compaction_job.hpp"
#include "lsmtree/sstable/build/row_sstable_builder.hpp"
#include "lsmtree/sstable/format/sst_layout.hpp"
#include "lsmtree/sstable/metadata/sstable_manifest.hpp"
#include "lsmtree/sstable/sstable_paths.hpp"

#include "storage/cursor/active_memtable_cursor.hpp"
#include "storage/cursor/cursor_factory.hpp"
#include "storage/cursor/immutable_memtable_cursor.hpp"
#include "storage/read/sstable/sstable_cursor_factory.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

namespace {

std::vector<ValueType> extract_schema_types(const Schema& schema) {
    std::vector<ValueType> types;
    types.reserve(schema.size());
    for (const auto& column : schema.columns()) {
        types.push_back(column.type);
    }
    return types;
}

uint64_t file_size_if_exists(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return 0;
    return std::filesystem::file_size(path);
}

uint64_t sstable_size_bytes(const std::filesystem::path& sstable_dir) {
    sstable::SSTablePaths paths(sstable_dir);
    return file_size_if_exists(paths.data())
         + file_size_if_exists(paths.meta())
         + file_size_if_exists(paths.index())
         + file_size_if_exists(paths.info());
}

/**
 * Курсор-обёртка, продлевающая жизнь backing-данных memtable.
 *
 * ImmutableMemTableCursor держат лишь итераторы в их вектор и НЕ владеют им. Фоновый worker может вытолкнуть
 * immutable из очереди и убрать её, пока scan ещё итерируется. Чтобы данные пережили курсор, PinnedCursor co-владеет
 * shared_ptr на используемые immutable memtable. Сами вызовы делегируются вложенному курсору.
 */
class PinnedCursor final : public ICursor {
public:
    PinnedCursor(
        std::unique_ptr<ICursor> inner,
        std::vector<MemoryLayer::ImmPtr> pins
    )
        : inner_(std::move(inner))
        , pins_(std::move(pins))
    {}

    bool valid() const override { return inner_->valid(); }
    void next() override { inner_->next(); }
    Key key() const override { return inner_->key(); }
    NullableValue value(std::size_t column_idx) const override {
        return inner_->value(column_idx);
    }

private:
    std::unique_ptr<ICursor> inner_;
    std::vector<MemoryLayer::ImmPtr> pins_;
};

} // namespace

LSMTree::LSMTree(const Schema& schema, const StorageConfig& config)
    : schema_(schema)
    , config_(config)
    , memory_layer_(config.memtable_threshold)
    , compaction_policy_(config_)
{
    std::filesystem::create_directories(config_.root_path);

    // Восстанавливаем реестр из manifest (если есть)
    sstable::SSTableManifest::load(config_.root_path, registry_, next_sst_id_);

    LOG_INFO("LSMTree initialized at path={}, next_sst_id={}", config_.root_path, next_sst_id_);

    // Запускаем фоновый worker-поток для compaction/flush задач
    worker_ = std::thread(&LSMTree::compaction_worker_loop, this);
}

LSMTree::~LSMTree() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    worker_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

const Schema& LSMTree::schema() const noexcept {
    return schema_;
}

void LSMTree::insert(const Row& row) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        memory_layer_.insert(row);
    }
    worker_cv_.notify_one();
}

std::unique_ptr<ICursor> LSMTree::scan(
    const storage::read::sstable::KeyRange& range,
    const std::vector<std::size_t>& projection,
    storage::ScanOrder order
) const {
    std::vector<std::unique_ptr<ICursor>> cursors;
    std::vector<MemoryLayer::ImmPtr> pins;

    {
        // Снимок состояния под mutex_

        std::lock_guard<std::mutex> lock(mutex_);

        // Active memtable
        const auto& active = memory_layer_.active();
        auto active_begin = range.from ? active.lower_bound(*range.from) : active.begin();
        auto active_end   = range.to   ? active.lower_bound(*range.to)   : active.end();
        if (active_begin != active_end) {
            cursors.push_back(std::make_unique<cursor::ActiveMemTableCursor>(
                active_begin, active_end
            ));
        }

        // Immutable memtables — co-владеем через shared_ptr (pins)
        for (const auto& immutable : memory_layer_.immutables()) {
            auto imm_begin = range.from ? immutable->lower_bound(*range.from) : immutable->begin();
            auto imm_end   = range.to   ? immutable->lower_bound(*range.to)   : immutable->end();
            if (imm_begin != imm_end) {
                cursors.push_back(std::make_unique<cursor::ImmutableMemTableCursor>(
                    imm_begin, imm_end
                ));
                pins.push_back(immutable);
            }
        }

        // SSTables (all levels)
        const auto schema_types = extract_schema_types(schema_);
        for (std::size_t level = 0; level < registry_.level_count(); ++level) {
            const auto sstables = registry_.overlapping(
                static_cast<uint32_t>(level),
                range.from,
                range.to
            );
            for (const auto& info : sstables) {
                auto cursor = read::sstable::make_sstable_cursor(
                    info, range, schema_types, projection
                );
                if (cursor && cursor->valid()) {
                    cursors.push_back(std::move(cursor));
                }
            }
        }
    }

    auto composed = cursor::compose_cursors(std::move(cursors), order);

    if (pins.empty()) {
        return composed;
    }
    return std::make_unique<PinnedCursor>(std::move(composed), std::move(pins));
}

void LSMTree::wait_for_compaction() {
    // Подталкиваем worker, чтобы не ждать конца интервала опроса.
    worker_cv_.notify_one();

    std::unique_lock<std::mutex> lock(mutex_);
    idle_cv_.wait(lock, [this] {
        return stop_ || (!worker_busy_ && !has_pending_work_locked());
    });
}

void LSMTree::compaction_worker_loop() {
    std::unique_lock<std::mutex> lock(mutex_);

    const auto interval = std::chrono::milliseconds(
        static_cast<long long>(config_.compaction_interval_ms)
    );

    while (true) {
        // Просыпаемся по таймауту или по сигналу
        worker_cv_.wait_for(lock, interval, [this] {
            return stop_ || has_pending_work_locked();
        });

        if (stop_) break;

        worker_busy_ = true;

        while (!stop_ && has_pending_work_locked()) {
            if (flush_one_locked(lock)) continue;
            if (compact_one_locked(lock)) continue;
            break;
        }

        worker_busy_ = false;

        if (!has_pending_work_locked()) {
            idle_cv_.notify_all();
        }
    }

    idle_cv_.notify_all();
}

bool LSMTree::has_pending_work_locked() const {
    if (memory_layer_.immutable_count() > 0) {
        return true;
    }
    return compaction_policy_.pick(registry_).has_value();
}

bool LSMTree::flush_one_locked(std::unique_lock<std::mutex>& lock) {
    auto imm = memory_layer_.front_immutable();
    if (!imm) return false;

    if (imm->size() == 0) {
        memory_layer_.pop_front_immutable();
        return true;
    }

    const uint64_t sst_id = next_sst_id_++;
    const std::string sst_path = build_sst_path(sst_id);

    LOG_INFO("Flushing SSTable id={} to {}", sst_id, sst_path);

    lock.unlock();

    sstable::RowSSTableBuilder builder(schema_, sst_path, config_.sparse_index_step);
    for (const auto& row : imm->data()) {
        builder.add(row);
    }
    const sstable::SSTableBuildResult build_result = builder.finish();
    const uint64_t file_size = sstable_size_bytes(sst_path);

    lock.lock();

    // flush всегда на level=0, layout=ROW.
    sstable::SSTableInfo info{
        .id              = sst_id,
        .path            = sst_path,
        .level           = 0,
        .min_key         = build_result.min_key,
        .max_key         = build_result.max_key,
        .file_size_bytes = file_size,
        .num_blocks      = build_result.num_blocks,
        .layout          = sstable::SSTLayout::ROW
    };

    registry_.add(info);
    memory_layer_.pop_front_immutable();
    save_manifest_locked();

    LOG_INFO("SSTable registered id={}, keys=[{}, {}]", info.id, info.min_key, info.max_key);

    return true;
}

bool LSMTree::compact_one_locked(std::unique_lock<std::mutex>& lock) {
    auto task = compaction_policy_.pick(registry_);
    if (!task) return false;

    const auto candidates = registry_.sstables_at_level(task->src_level);
    if (candidates.empty()) return false;

    const uint64_t new_id = next_sst_id_++;

    LOG_INFO(
        "Compaction triggered: L{}→L{}, candidates={}",
        task->src_level, task->dst_level, candidates.size()
    );

    lock.unlock();

    CompactionJob job(schema_, config_.root_path);
    const sstable::SSTableInfo new_info = job.run(candidates, *task, new_id);

    lock.lock();

    registry_.add(new_info);
    for (const auto& old_sst : candidates) {
        registry_.remove(old_sst.id);
        delete_sst_files(old_sst.id);
    }

    save_manifest_locked();

    LOG_INFO(
        "Compaction done: new SST id={}, L{}, layout={}, keys=[{}, {}]",
        new_info.id, new_info.level,
        new_info.layout == sstable::SSTLayout::COLUMN ? "COLUMN" : "ROW",
        new_info.min_key, new_info.max_key
    );

    return true;
}

void LSMTree::save_manifest_locked() const {
    sstable::SSTableManifest::save(config_.root_path, registry_, next_sst_id_);
}

std::string LSMTree::build_sst_path(uint64_t id) const {
    return std::format("{}/sst_{:020}.sst", config_.root_path, id);
}

void LSMTree::delete_sst_files(uint64_t sst_id) const {
    const std::string sst_dir = build_sst_path(sst_id);
    if (std::filesystem::exists(sst_dir)) {
        std::filesystem::remove_all(sst_dir);
        LOG_INFO("Deleted SSTable files: {}", sst_dir);
    }
}

