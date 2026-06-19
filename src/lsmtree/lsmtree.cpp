#include "lsmtree/lsmtree.hpp"

#include <filesystem>
#include <stdexcept>
#include <limits>
#include <format>

#include "utils/logger.hpp"
#include "lsmtree/sstable/format/sst_layout.hpp"
#include "lsmtree/sstable/build/sstable_builder.hpp"
#include "lsmtree/sstable/sstable_paths.hpp"
#include "storage/cursor/active_memtable_cursor.hpp"
#include "storage/cursor/immutable_memtable_cursor.hpp"
#include "storage/cursor/cursor_factory.hpp"
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

std::uint64_t file_size_if_exists(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return 0;
    }
    return std::filesystem::file_size(path);
}

std::uint64_t sstable_size_bytes(const std::filesystem::path& sstable_dir) {
    htap::lsmtree::sstable::SSTablePaths paths(sstable_dir);

    return file_size_if_exists(paths.data())
        + file_size_if_exists(paths.meta())
        + file_size_if_exists(paths.index())
        + file_size_if_exists(paths.info());
}

}

LSMTree::LSMTree(const Schema& schema, const std::string& path, size_t memtable_threshold)
    : schema_(schema)
    , path_(path)
    , memory_layer_(memtable_threshold)
    , next_sst_id_(0)
{
    std::filesystem::create_directories(path_);

    LOG_INFO("LSMTree initialized at path={}", path_);
}

const Schema& LSMTree::schema() const noexcept {
    return schema_;
}

void LSMTree::insert(const Row& row) {
    memory_layer_.insert(row);
    flush_memtable(); // пока всегда при появлении immutable он сразу будет flush-иться
    // в будущем возможно будем хранить их несколько и только в какой то момент flush-ить
}

void LSMTree::flush_memtable() {
    auto imm = memory_layer_.pop_immutable();
    if (!imm)
        return;

    uint64_t sst_id = next_sst_id_++;
    std::string file_path = build_sst_path(sst_id);

    LOG_INFO("Flushing SSTable id={} to {}", sst_id, file_path);

    sstable::SSTableBuilder builder(schema_, file_path);

    for (const auto& row : imm->data()) {
        builder.add(row);
    }

    sstable::SSTableBuildResult build_result = builder.finish();

    // эти две переменные - захардкожено то, куда и в каком виде должно
    // попадать первый sstable при flush-е imm_memtabl-а
    uint32_t level = 0;
    sstable::SSTLayout layout = sstable::SSTLayout::ROW;

    sstable::SSTableInfo info{
        .id = sst_id,
        .path = file_path,
        .level = level,
        .min_key = build_result.min_key,
        .max_key = build_result.max_key,
        .file_size_bytes = sstable_size_bytes(file_path),
        .num_blocks = build_result.num_blocks,
        .layout = layout
    };

    registry_.add(info);

    LOG_INFO("SSTable registered id={}, keys belong [{}, {}]", info.id, info.min_key, info.max_key);
}

std::string LSMTree::build_sst_path(uint64_t id) const {
    return std::format("{}/sst_{:020}.sst", path_, id);
}

std::unique_ptr<ICursor> LSMTree::scan(
    const storage::read::sstable::KeyRange& range,
    const std::vector<std::size_t>& projection,
    storage::ScanOrder order
) const {
    std::vector<std::unique_ptr<ICursor>> cursors;

    const auto& active = memory_layer_.active();

    auto active_begin = range.from ? active.lower_bound(*range.from) : active.begin();

    auto active_end = range.to ? active.lower_bound(*range.to) : active.end();

    if (active_begin != active_end) {
        cursors.push_back(std::make_unique<cursor::ActiveMemTableCursor>(
            active_begin,
            active_end
        ));
    }

    for (const auto& immutable : memory_layer_.immutables()) {
        auto immutable_begin = range.from
            ? immutable->lower_bound(*range.from)
            : immutable->begin();

        auto immutable_end = range.to
            ? immutable->lower_bound(*range.to)
            : immutable->end();

        if (immutable_begin != immutable_end) {
            cursors.push_back(std::make_unique<cursor::ImmutableMemTableCursor>(
                immutable_begin,
                immutable_end
            ));
        }
    }

    const auto schema_types = extract_schema_types(schema_);

    for (std::size_t level = 0; level < registry_.level_count(); ++level) {
        const auto sstables = registry_.overlapping(
            static_cast<std::uint32_t>(level),
            range.from,
            range.to
        );

        for (const auto& info : sstables) {
            auto cursor = read::sstable::make_sstable_cursor(
                info,
                range,
                schema_types,
                projection
            );

            if (cursor && cursor->valid()) {
                cursors.push_back(std::move(cursor));
            }
        }
    }

    return cursor::compose_cursors(
        std::move(cursors),
        order
    );

}
