#include "lsmtree/compaction/compaction_job.hpp"

#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>
#include <vector>

#include "lsmtree/sstable/build/column_sstable_builder.hpp"
#include "lsmtree/sstable/build/row_sstable_builder.hpp"
#include "lsmtree/sstable/format/sst_layout.hpp"

#include "storage/api/cursor_interface.hpp"
#include "storage/api/types.hpp"
#include "storage/cursor/merge_cursor.hpp"
#include "storage/read/sstable/key_range.hpp"
#include "storage/read/sstable/sstable_cursor_factory.hpp"

#include "utils/logger.hpp"

using namespace htap::lsmtree;
using namespace htap::lsmtree::sstable;
using namespace htap::storage;

namespace {

// Вычисляет суммарный размер директории SSTable.
uint64_t sstable_dir_size(const std::filesystem::path& dir) {
    uint64_t total = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            total += entry.file_size();
        }
    }
    return total;
}

// Извлекает типы колонок из схемы.
std::vector<ValueType> schema_types(const Schema& schema) {
    std::vector<ValueType> types;
    types.reserve(schema.size());
    for (const auto& col : schema.columns()) {
        types.push_back(col.type);
    }
    return types;
}

} // namespace

CompactionJob::CompactionJob(const Schema& schema, const std::string& table_path)
    : schema_(schema)
    , table_path_(table_path)
{}

std::string CompactionJob::build_sst_path(uint64_t sst_id) const {
    return std::format("{}/sst_{:020}.sst", table_path_, sst_id);
}

SSTableInfo CompactionJob::run(
    const std::vector<SSTableInfo>& candidates,
    const CompactionTask& task,
    uint64_t new_sst_id
) {
    if (candidates.empty())
        throw std::runtime_error("CompactionJob::run: no candidates");

    LOG_INFO(
        "CompactionJob: start compaction L{}→L{}, layout={}, candidates={}",
        task.src_level, task.dst_level,
        task.output_layout == SSTLayout::COLUMN ? "COLUMN" : "ROW",
        candidates.size()
    );

    // 1. Открываем cursor на каждый кандидат и мержим через MergeCursor
    const auto types        = schema_types(schema_);
    const auto all_columns  = [&]() {
        std::vector<std::size_t> proj;
        proj.reserve(schema_.size());
        for (std::size_t i = 0; i < schema_.size(); ++i) proj.push_back(i);
        return proj;
    }();

    const read::sstable::KeyRange full_range{std::nullopt, std::nullopt};

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.reserve(candidates.size());

    for (const auto& sst : candidates) {
        auto cursor = read::sstable::make_sstable_cursor(sst, full_range, types, all_columns);
        if (cursor && cursor->valid()) {
            cursors.push_back(std::move(cursor));
        }
    }

    if (cursors.empty())
        throw std::runtime_error("CompactionJob::run: all candidate cursors are empty");

    auto merged = std::make_unique<cursor::MergeCursor>(std::move(cursors));

    // 2. Пишем новый SST с нужным layout
    const std::string new_path = build_sst_path(new_sst_id);

    SSTableBuildResult build_result;

    if (task.output_layout == SSTLayout::COLUMN) {
        ColumnSSTableBuilder builder(schema_, new_path);
        while (merged->valid()) {
            Row row(schema_.size());
            row[KEY_COLUMN_INDEX] = merged->key();
            for (std::size_t ci = 1; ci < schema_.size(); ++ci) {
                row[ci] = merged->value(ci);
            }
            builder.add(row);
            merged->next();
        }
        build_result = builder.finish();
    } else {
        RowSSTableBuilder builder(schema_, new_path);
        while (merged->valid()) {
            Row row(schema_.size());
            row[KEY_COLUMN_INDEX] = merged->key();
            for (std::size_t ci = 1; ci < schema_.size(); ++ci) {
                row[ci] = merged->value(ci);
            }
            builder.add(row);
            merged->next();
        }
        build_result = builder.finish();
    }

    // 3. Формируем SSTableInfo нового SST
    const uint64_t file_size = sstable_dir_size(new_path);

    SSTableInfo new_info{
        .id              = new_sst_id,
        .path            = new_path,
        .level           = task.dst_level,
        .min_key         = build_result.min_key,
        .max_key         = build_result.max_key,
        .file_size_bytes = file_size,
        .num_blocks      = build_result.num_blocks,
        .layout          = task.output_layout
    };

    LOG_INFO(
        "CompactionJob: finished new SST id={}, level={}, layout={}, "
        "keys=[{}, {}], blocks={}, size={}",
        new_info.id, new_info.level,
        task.output_layout == SSTLayout::COLUMN ? "COLUMN" : "ROW",
        new_info.min_key, new_info.max_key,
        new_info.num_blocks, new_info.file_size_bytes
    );

    return new_info;
}
