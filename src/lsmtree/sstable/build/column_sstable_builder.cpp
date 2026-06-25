#include "lsmtree/sstable/build/column_sstable_builder.hpp"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <stdexcept>

#include "lsmtree/sstable/format/sparse_index_entry.hpp"
#include "lsmtree/sstable/format/sst_layout.hpp"
#include "utils/logger.hpp"

using namespace htap::storage;
using namespace htap::lsmtree::sstable;

ColumnSSTableBuilder::ColumnSSTableBuilder(
    const Schema& schema,
    const std::filesystem::path& sstable_dir,
    uint32_t sparse_index_step
)
    : schema_(schema)
    , paths_(sstable_dir)
    , data_writer_(data_file_)
    , meta_writer_(meta_file_)
    , index_writer_(index_file_)
    , sparse_index_step_(sparse_index_step)
    , global_min_(std::numeric_limits<Key>::max())
    , global_max_(std::numeric_limits<Key>::min())
    , last_key_(0)
    , first_row_(true)
{
    if (schema_.size() == 0)
        throw std::runtime_error("ColumnSSTableBuilder: schema must not be empty");

    std::filesystem::create_directories(paths_.dir());

    data_file_.open(paths_.data(), std::ios::binary);
    meta_file_.open(paths_.meta(), std::ios::binary);
    index_file_.open(paths_.index(), std::ios::binary);

    if (!data_file_ || !meta_file_ || !index_file_)
        throw std::runtime_error("ColumnSSTableBuilder: failed to open files in " +
                                 paths_.dir().string());

    // Создаём builders для всех value-колонок (индексы 1..N-1 в схеме)
    // Колонка 0 — ключ, обрабатывается отдельно как packed int64 (без bitmap)
    col_builders_.reserve(schema_.size() - 1);
    for (size_t i = 1; i < schema_.size(); ++i) {
        col_builders_.emplace_back(schema_.get_column(i), static_cast<uint16_t>(i));
    }

    LOG_INFO("ColumnSSTableBuilder: opened {}", paths_.dir().string());
}

void ColumnSSTableBuilder::add(const Row& row) {
    if (finished_)
        throw std::runtime_error("ColumnSSTableBuilder: already finished");

    const Key key = std::get<Key>(*row[KEY_COLUMN_INDEX]);

    if (!first_row_ && key < last_key_)
        throw std::runtime_error("ColumnSSTableBuilder: rows must be sorted by key");

    if (key_buffer_.size() >= TARGET_BLOCK_ROWS || any_col_builder_full())
        flush_block();

    key_buffer_.push_back(key);

    for (size_t i = 1; i < schema_.size(); ++i) {
        col_builders_[i - 1].add(row);
    }

    first_row_ = false;
    last_key_ = key;

    global_min_ = std::min(global_min_, key);
    global_max_ = std::max(global_max_, key);
}

SSTableBuildResult ColumnSSTableBuilder::finish() {
    if (finished_)
        throw std::runtime_error("ColumnSSTableBuilder: already finished");

    if (first_row_)
        throw std::runtime_error("ColumnSSTableBuilder: cannot create empty SSTable");

    if (!key_buffer_.empty())
        flush_block();

    write_info_file();

    data_file_.flush();
    meta_file_.flush();
    index_file_.flush();

    finished_ = true;

    LOG_INFO(
        "ColumnSSTableBuilder finished: blocks={}, min_key={}, max_key={}",
        block_id_,
        global_min_,
        global_max_
    );

    return SSTableBuildResult{
        .min_key = global_min_,
        .max_key = global_max_,
        .num_blocks = block_id_
    };
}

bool ColumnSSTableBuilder::any_col_builder_full() const {
    for (const auto& b : col_builders_) {
        if (b.full()) return true;
    }
    return false;
}

void ColumnSSTableBuilder::write_block_meta(
    Key min_key,
    Key max_key,
    uint64_t offset,
    uint64_t size_bytes,
    uint32_t values_count,
    uint32_t block_id,
    uint32_t column_idx
) {
    meta_writer_.write_i64(min_key);
    meta_writer_.write_i64(max_key);
    meta_writer_.write_u64(offset);
    meta_writer_.write_u64(size_bytes);
    meta_writer_.write_u32(values_count);
    meta_writer_.write_u32(block_id);
    meta_writer_.write_u32(column_idx);
}

void ColumnSSTableBuilder::flush_block() {
    if (key_buffer_.empty()) return;

    const uint32_t row_count = static_cast<uint32_t>(key_buffer_.size());
    const Key block_min = key_buffer_.front();
    const Key block_max = key_buffer_.back();

    // 1. Пишем KeyBlock (column_idx = 0): packed int64[], без null-bitmap
    {
        const uint64_t key_block_offset = data_offset_;
        const uint64_t key_block_size   = sizeof(Key) * row_count;

        for (Key k : key_buffer_) {
            data_writer_.write_i64(k);
        }
        data_offset_ += key_block_size;

        write_block_meta(
            block_min, block_max,
            key_block_offset, key_block_size,
            row_count,
            block_id_,
            KEY_COLUMN_INDEX  // column_idx = 0
        );
    }

    // 2. Пишем ColBlock для каждой value-колонки (column_idx = 1..N-1)
    for (size_t ci = 0; ci < col_builders_.size(); ++ci) {
        auto result = col_builders_[ci].finish();  // автоматически делает reset

        const uint64_t col_block_offset = data_offset_;
        const uint64_t col_block_size   = static_cast<uint64_t>(result.data.size());

        data_writer_.write_bytes(result.data.data(), result.data.size());
        data_offset_ += col_block_size;

        const uint32_t schema_col_idx = static_cast<uint32_t>(ci + 1);

        write_block_meta(
            result.meta.min_key, result.meta.max_key,
            col_block_offset, col_block_size,
            result.meta.values_count,
            block_id_,
            schema_col_idx
        );
    }

    // 3. Sparse index entry (каждые sparse_index_step_ logical blocks)
    if (block_id_ % sparse_index_step_ == 0) {
        SparseIndexEntry entry{
            .min_key  = block_min,
            .block_id = block_id_
        };
        index_writer_.write_i64(entry.min_key);
        index_writer_.write_u32(entry.block_id);
    }

    LOG_INFO(
        "ColumnSSTableBuilder: flushed block id={}, rows={}, min_key={}, max_key={}",
        block_id_, row_count, block_min, block_max
    );

    key_buffer_.clear();
    ++block_id_;
}

void ColumnSSTableBuilder::write_info_file() {
    std::ofstream info_file(paths_.info(), std::ios::binary);
    if (!info_file)
        throw std::runtime_error("ColumnSSTableBuilder: failed to open info file: " +
                                 paths_.info().string());

    utils::BinaryWriter writer(info_file);
    writer.write_u32(block_id_);
    writer.write_i64(global_min_);
    writer.write_i64(global_max_);
    writer.write_u8(static_cast<uint8_t>(SSTLayout::COLUMN));

    info_file.flush();
}
