#include "lsmtree/sstable/sstable_builder.hpp"

#include <limits>
#include <stdexcept>

#include "lsmtree/sstable/format/sstable_info.hpp"
#include "lsmtree/sstable/format/sparse_index_entry.hpp"

#include "utils/logger.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

SSTableBuilder::SSTableBuilder(
    const Schema& schema,
    const std::filesystem::path& sstable_dir,
    uint32_t sparse_index_step
)
    : schema_(schema)
    , paths_(sstable_dir)
    , data_file_(paths_.data(), std::ios::binary)
    , meta_file_(paths_.meta(), std::ios::binary)
    , index_file_(paths_.index(), std::ios::binary)
    , data_writer_(data_file_)
    , meta_writer_(meta_file_)
    , index_writer_(index_file_)
    , block_builder_(schema)
    , sparse_index_step_(sparse_index_step)
    , global_min_(std::numeric_limits<Key>::max())
    , global_max_(std::numeric_limits<Key>::min())
    , last_key_(0)
    , first_row_(true)
{
    if (!data_file_ || !meta_file_ || !index_file_)
        throw std::runtime_error("Failed to open SSTable file");

    LOG_INFO("SSTableBuilder: opened {}", paths_.dir().string());
}

void SSTableBuilder::add(const Row& row) {
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

void SSTableBuilder::flush_block() {
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

    meta_writer_.write_i64(meta.min_key);
    meta_writer_.write_i64(meta.max_key);
    meta_writer_.write_u64(meta.offset);
    meta_writer_.write_u64(meta.size_bytes);
    meta_writer_.write_u32(meta.row_count);
    meta_writer_.write_u32(meta.block_id);

    // SPARSE INDEX

    if (block_id_ % sparse_index_step_ == 0) {
        format::SparseIndexEntry sparse_idx_entry{
            .min_key = meta.min_key,
            .block_id = meta.block_id
        };
        index_writer_.write_i64(sparse_idx_entry.min_key);
        index_writer_.write_u32(sparse_idx_entry.block_id);
    }

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

void SSTableBuilder::write_info_file() {
    std::ofstream info_file(paths_.info(), std::ios::binary);

    if (!info_file)
        throw std::runtime_error("Failed to open SSTable info file: " + paths_.info().string());

    utils::BinaryWriter writer(info_file);

    format::SSTableInfo info;

    info.num_blocks = block_id_;
    info.min_key = global_min_;
    info.max_key = global_max_;
    info.layout_type = format::SSTLayout::ROW;

    writer.write_u32(info.magic);
    writer.write_u32(info.num_blocks);
    writer.write_i64(info.min_key);
    writer.write_i64(info.max_key);
    writer.write_u8(static_cast<uint8_t>(info.layout_type));

    info_file.flush();
}

void SSTableBuilder::finish() {
    if (finished_)
        return;

    if (first_row_)
        throw std::runtime_error("Cannot create empty SSTable");

    if (block_builder_.size_bytes() > 0)
        flush_block();

    write_info_file();

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
}
