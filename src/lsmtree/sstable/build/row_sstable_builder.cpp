#include "lsmtree/sstable/build/row_sstable_builder.hpp"

#include <filesystem>
#include <algorithm>
#include <limits>
#include <stdexcept>

#include "lsmtree/sstable/format/sparse_index_entry.hpp"
#include "lsmtree/sstable/format/sst_layout.hpp"

#include "utils/logger.hpp"

using namespace htap::storage;
using namespace htap::lsmtree::sstable;

RowSSTableBuilder::RowSSTableBuilder(
    const Schema& schema,
    const std::filesystem::path& sstable_dir,
    uint32_t sparse_index_step,
    std::size_t row_block_target_bytes
)
    : RowSSTableBuilder(
        schema,
        sstable_dir,
        SparseIndexOptions{.fixed_step = sparse_index_step},
        row_block_target_bytes
    )
{}

RowSSTableBuilder::RowSSTableBuilder(
    const Schema& schema,
    const std::filesystem::path& sstable_dir,
    SparseIndexOptions sparse_index_options,
    std::size_t row_block_target_bytes
)
    : schema_(schema)
    , paths_(sstable_dir)
    , data_writer_(data_file_)
    , meta_writer_(meta_file_)
    , index_writer_(index_file_)
    , block_builder_(schema, row_block_target_bytes)
    , sparse_index_options_(sparse_index_options)
    , global_min_(std::numeric_limits<Key>::max())
    , global_max_(std::numeric_limits<Key>::min())
    , last_key_(0)
    , first_row_(true)
{
    if (sparse_index_options_.min_step == 0) {
        throw std::runtime_error("RowSSTableBuilder: sparse index min step must be positive");
    }

    if (sparse_index_options_.max_step < sparse_index_options_.min_step) {
        throw std::runtime_error("RowSSTableBuilder: sparse index max step must be >= min step");
    }

    std::filesystem::create_directories(paths_.dir());

    data_file_.open(paths_.data(), std::ios::binary);
    meta_file_.open(paths_.meta(), std::ios::binary);
    index_file_.open(paths_.index(), std::ios::binary);

    if (!data_file_ || !meta_file_ || !index_file_)
        throw std::runtime_error("Failed to open SSTable file");

    LOG_INFO("SSTableBuilder: opened {}", paths_.dir().string());
}

void RowSSTableBuilder::add(const Row& row) {
    if (finished_)
        throw std::runtime_error("SSTable already finished");

    if (block_builder_.full())
        flush_block();

    const Key key = std::get<Key>(*row[KEY_COLUMN_INDEX]);

    if (!first_row_ && key < last_key_)
        throw std::runtime_error("SSTableBuilder: rows must be sorted by key");

    block_builder_.add(row);

    LOG_DEBUG("SSTableBuilder: add row with key={}", key);

    first_row_ = false;
    last_key_ = key;

    global_min_ = std::min(global_min_, key);
    global_max_ = std::max(global_max_, key);
}

void RowSSTableBuilder::flush_block() {
    auto result = block_builder_.finish();

    const uint64_t block_offset = data_offset_;

    // DATA

    data_offset_ += data_writer_.write_bytes(
        result.data.data(),
        result.data.size()
    );

    // META

    RowBlockMeta meta = result.meta;

    meta.offset = block_offset;
    meta.block_id = block_id_;
    block_numeric_stats_.push_back(std::move(result.numeric_stats));

    meta_writer_.write_i64(meta.min_key);
    meta_writer_.write_i64(meta.max_key);
    meta_writer_.write_u64(meta.offset);
    meta_writer_.write_u64(meta.size_bytes);
    meta_writer_.write_u32(meta.row_count);
    meta_writer_.write_u32(meta.block_id);

    sparse_index_candidates_.push_back(SparseIndexEntry{
        .min_key = meta.min_key,
        .block_id = meta.block_id
    });

    LOG_INFO(
        "SSTableBuilder: flushed block id={}, rows={}, min_key={}, max_key={}, size={}",
        meta.block_id,
        meta.row_count,
        meta.min_key,
        meta.max_key,
        meta.size_bytes
    );

    ++block_id_;
}

void RowSSTableBuilder::write_info_file() {
    std::ofstream info_file(paths_.info(), std::ios::binary);

    if (!info_file)
        throw std::runtime_error("Failed to open SSTable info file: " + paths_.info().string());

    utils::BinaryWriter writer(info_file);

    writer.write_u32(block_id_);
    writer.write_i64(global_min_);
    writer.write_i64(global_max_);
    writer.write_u8(static_cast<uint8_t>(SSTLayout::ROW));

    info_file.flush();
}

void RowSSTableBuilder::write_sparse_index_file() {
    const auto effective_step = choose_sparse_index_step(block_id_, sparse_index_options_);

    for (const auto& entry : sparse_index_candidates_) {
        if (entry.block_id % effective_step != 0) {
            continue;
        }

        index_writer_.write_i64(entry.min_key);
        index_writer_.write_u32(entry.block_id);
    }

    index_file_.flush();
}

void RowSSTableBuilder::write_stats_file() {
    std::ofstream stats_file(paths_.stats(), std::ios::binary);

    if (!stats_file) {
        throw std::runtime_error("Failed to open SSTable stats file: " + paths_.stats().string());
    }

    utils::BinaryWriter writer(stats_file);

    struct StatsColumn {
        std::size_t column_idx;
        ValueType type;
    };

    std::vector<StatsColumn> stats_columns;
    for (std::size_t column_idx = 0; column_idx < schema_.size(); ++column_idx) {
        const auto& column = schema_.get_column(column_idx);
        if (!column.is_key && read::sstable::is_numeric_type(column.type)) {
            stats_columns.push_back(StatsColumn{
                .column_idx = column_idx,
                .type = column.type
            });
        }
    }

    constexpr std::uint32_t STATS_MAGIC = 0x53544154; // "STAT"
    constexpr std::uint32_t STATS_VERSION = 1;
    constexpr std::uint32_t STATS_HEADER_SIZE = 16;
    constexpr std::uint32_t STATS_COLUMN_DESCRIPTOR_SIZE = 17;
    constexpr std::uint32_t NUMERIC_STATS_ENTRY_SIZE = 17;

    const auto num_blocks = static_cast<std::uint32_t>(block_numeric_stats_.size());
    const auto num_stats_columns = static_cast<std::uint32_t>(stats_columns.size());
    const auto data_start_offset =
        static_cast<std::uint64_t>(STATS_HEADER_SIZE) +
        static_cast<std::uint64_t>(num_stats_columns) * STATS_COLUMN_DESCRIPTOR_SIZE;

    writer.write_u32(STATS_MAGIC);
    writer.write_u32(STATS_VERSION);
    writer.write_u32(num_blocks);
    writer.write_u32(num_stats_columns);

    for (std::uint32_t stats_column_idx = 0; stats_column_idx < num_stats_columns; ++stats_column_idx) {
        const auto& column = stats_columns[stats_column_idx];
        const auto column_offset =
            data_start_offset +
            static_cast<std::uint64_t>(stats_column_idx) *
                static_cast<std::uint64_t>(num_blocks) *
                NUMERIC_STATS_ENTRY_SIZE;

        writer.write_u32(static_cast<std::uint32_t>(column.column_idx));
        writer.write_u8(static_cast<std::uint8_t>(column.type));
        writer.write_u64(column_offset);
        writer.write_u32(NUMERIC_STATS_ENTRY_SIZE);
    }

    auto find_block_stats = [](
        const std::vector<read::sstable::NumericBlockStats>& block_stats,
        std::size_t column_idx
    ) -> const read::sstable::NumericBlockStats* {
        const auto it = std::find_if(
            block_stats.begin(),
            block_stats.end(),
            [column_idx](const auto& stats) {
                return stats.column_idx == column_idx;
            }
        );

        return it == block_stats.end() ? nullptr : &*it;
    };

    for (const auto& column : stats_columns) {
        for (const auto& block_stats : block_numeric_stats_) {
            const auto* stats = find_block_stats(block_stats, column.column_idx);

            writer.write_u8(stats && stats->has_value ? 1 : 0);

            if (column.type == ValueType::INT64) {
                const auto min_value = stats && stats->has_value
                    ? std::get<std::int64_t>(stats->min_value)
                    : std::int64_t{0};
                const auto max_value = stats && stats->has_value
                    ? std::get<std::int64_t>(stats->max_value)
                    : std::int64_t{0};

                writer.write_i64(min_value);
                writer.write_i64(max_value);
                continue;
            }

            if (column.type == ValueType::DOUBLE) {
                const auto min_value = stats && stats->has_value
                    ? std::get<double>(stats->min_value)
                    : 0.0;
                const auto max_value = stats && stats->has_value
                    ? std::get<double>(stats->max_value)
                    : 0.0;

                writer.write_double(min_value);
                writer.write_double(max_value);
            }
        }
    }

    stats_file.flush();
}

SSTableBuildResult RowSSTableBuilder::finish() {
    if (finished_)
        throw std::runtime_error("Already finished building SSTable");

    if (first_row_)
        throw std::runtime_error("Cannot create empty SSTable");

    if (block_builder_.size_bytes() > 0)
        flush_block();

    write_info_file();
    write_sparse_index_file();
    write_stats_file();

    data_file_.flush();
    meta_file_.flush();
    index_file_.flush();

    finished_ = true;

    LOG_INFO(
        "SSTableBuilder finished: blocks={}, min_key={}, max_key={}",
        block_id_,
        global_min_,
        global_max_
    );

    SSTableBuildResult result {
        .min_key = global_min_,
        .max_key = global_max_,
        .num_blocks = block_id_
    };

    return result;
}
