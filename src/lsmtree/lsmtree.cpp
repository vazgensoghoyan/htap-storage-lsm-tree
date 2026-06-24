#include "lsmtree/lsmtree.hpp"

#include <filesystem>
#include <format>
#include <limits>
#include <stdexcept>
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

} // namespace

LSMTree::LSMTree(const Schema& schema, const StorageConfig& config)
    : schema_(schema)
    , config_(config)
    , memory_layer_(config.memtable_threshold)
    , compaction_policy_(config)
{
    std::filesystem::create_directories(config_.root_path);

    // Восстанавливаем реестр из manifest (если есть)
    sstable::SSTableManifest::load(config_.root_path, registry_, next_sst_id_);

    LOG_INFO("LSMTree initialized at path={}, next_sst_id={}", config_.root_path, next_sst_id_);
}

const Schema& LSMTree::schema() const noexcept {
    return schema_;
}

void LSMTree::insert(const Row& row) {
    memory_layer_.insert(row);
    flush_memtable();
}

std::unique_ptr<ICursor> LSMTree::scan(
    const storage::read::sstable::KeyRange& range,
    const std::vector<std::size_t>& projection,
    storage::ScanOrder order
) const {
    std::vector<std::unique_ptr<ICursor>> cursors;

    // Active memtable
    const auto& active = memory_layer_.active();
    auto active_begin = range.from ? active.lower_bound(*range.from) : active.begin();
    auto active_end   = range.to   ? active.lower_bound(*range.to)   : active.end();
    if (active_begin != active_end) {
        cursors.push_back(std::make_unique<cursor::ActiveMemTableCursor>(
            active_begin, active_end
        ));
    }

    // Immutable memtables
    for (const auto& immutable : memory_layer_.immutables()) {
        auto imm_begin = range.from ? immutable->lower_bound(*range.from) : immutable->begin();
        auto imm_end   = range.to   ? immutable->lower_bound(*range.to)   : immutable->end();
        if (imm_begin != imm_end) {
            cursors.push_back(std::make_unique<cursor::ImmutableMemTableCursor>(
                imm_begin, imm_end
            ));
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

    return cursor::compose_cursors(std::move(cursors), order);
}

void LSMTree::flush_memtable() {
    auto imm = memory_layer_.pop_immutable();
    if (!imm) return;

    const uint64_t sst_id = next_sst_id_++;
    const std::string sst_path = build_sst_path(sst_id);

    LOG_INFO("Flushing SSTable id={} to {}", sst_id, sst_path);

    sstable::RowSSTableBuilder builder(schema_, sst_path);
    for (const auto& row : imm->data()) {
        builder.add(row);
    }
    sstable::SSTableBuildResult build_result = builder.finish();

    // Flush всегда идёт на level=0, layout=ROW
    const uint32_t level        = 0;
    const sstable::SSTLayout layout = sstable::SSTLayout::ROW;

    sstable::SSTableInfo info{
        .id              = sst_id,
        .path            = sst_path,
        .level           = level,
        .min_key         = build_result.min_key,
        .max_key         = build_result.max_key,
        .file_size_bytes = sstable_size_bytes(sst_path),
        .num_blocks      = build_result.num_blocks,
        .layout          = layout
    };

    registry_.add(info);
    save_manifest();

    LOG_INFO("SSTable registered id={}, keys=[{}, {}]", info.id, info.min_key, info.max_key);

    maybe_compact();
}

void LSMTree::maybe_compact() {
    auto task = compaction_policy_.pick(registry_);
    if (!task) return;

    const auto candidates = registry_.sstables_at_level(task->src_level);
    if (candidates.empty()) return;

    LOG_INFO(
        "Compaction triggered: L{}→L{}, candidates={}",
        task->src_level, task->dst_level, candidates.size()
    );

    CompactionJob job(schema_, config_.root_path);
    const uint64_t new_id = next_sst_id_++;

    sstable::SSTableInfo new_info = job.run(candidates, *task, new_id);

    // Атомарная замена в реестре
    registry_.add(new_info);
    for (const auto& old_sst : candidates) {
        registry_.remove(old_sst.id);
        delete_sst_files(old_sst.id);
    }

    save_manifest();

    LOG_INFO(
        "Compaction done: new SST id={}, L{}, layout={}, keys=[{}, {}]",
        new_info.id, new_info.level,
        new_info.layout == sstable::SSTLayout::COLUMN ? "COLUMN" : "ROW",
        new_info.min_key, new_info.max_key
    );

    // Рекурсивно проверяем нужен ли ещё compaction (cascade)
    maybe_compact();
}

void LSMTree::save_manifest() const {
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
