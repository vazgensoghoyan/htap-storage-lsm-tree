#include "lsmtree/sstable/sstable_builder.hpp"

#include <limits>
#include <stdexcept>
#include "utils/logger.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

SSTableBuilder::SSTableBuilder(const Schema& schema, const std::string& path)
    : schema_(schema)
    , file_(path, std::ios::binary)
    , writer_(file_)
    , block_builder_(schema)
    , global_min_(std::numeric_limits<int64_t>::max())
    , global_max_(std::numeric_limits<int64_t>::min())
    , last_key_(0)
    , first_row_(true)
{
    if (!file_)
        throw std::runtime_error("Failed to open SSTable file");

    LOG_INFO("SSTableBuilder: opened file {}", path);
}

void SSTableBuilder::add(const Row& row) {
    if (finished_)
        throw std::runtime_error("SSTable already finished");

    if (block_builder_.full())
        flush_block();

    Key key = std::get<Key>(*row[KEY_COLUMN_INDEX]);

    if (!first_row_ && key < last_key_)
        throw std::runtime_error("SSTableBuilder: input rows are not sorted by key");

    block_builder_.add(row);

    LOG_DEBUG("SSTableBuilder: add row with key={}", key);

    first_row_ = false;
    last_key_ = key;

    global_min_ = std::min(global_min_, key);
    global_max_ = std::max(global_max_, key);
}

void SSTableBuilder::flush_block() {
    auto result = block_builder_.finish();
    uint64_t offset = file_offset_;

    writer_.write_bytes(result.data.data(), result.data.size());
    file_offset_ += result.data.size();

    RowBlockMeta meta = result.meta;
    meta.offset = offset;
    meta.block_id = block_id_++;

    meta_.push_back(meta);

    LOG_INFO("SSTableBuilder: flush block id={}, rows={}, min_key={}, max_key={}, size_bytes={}",
        meta.block_id,
        meta.row_count,
        meta.min_key,
        meta.max_key,
        meta.size_bytes);
}

void SSTableBuilder::finish() {
    if (finished_)
        return;

    LOG_INFO("SSTableBuilder: finish start");

    LOG_DEBUG("Finish state: file_offset={}, pending_meta={}, current_block_size={}",
              file_offset_, meta_.size(), block_builder_.size_bytes());

    if (block_builder_.size_bytes() > 0) {
        LOG_INFO("Flushing last block before finish (size_bytes={})",
                 block_builder_.size_bytes());
        flush_block();
    }

    LOG_INFO("Writing meta section: blocks={}, meta_offset={}",
             meta_.size(), file_offset_);

    uint64_t meta_offset = file_offset_;

    for (const auto& m : meta_) {
        LOG_DEBUG("Write meta block: id={}, min_key={}, max_key={}, rows={}, size_bytes={}, offset={}",
                m.block_id,
                m.min_key,
                m.max_key,
                m.row_count,
                m.size_bytes,
                m.offset);

        writer_.write_u64(static_cast<uint64_t>(m.min_key));
        writer_.write_u64(static_cast<uint64_t>(m.max_key));
        writer_.write_u32(m.row_count);
        writer_.write_u64(m.offset);
        writer_.write_u64(m.size_bytes);
        writer_.write_u32(m.block_id);

        file_offset_ += sizeof(m.min_key)
                      + sizeof(m.max_key)
                      + sizeof(m.row_count)
                      + sizeof(m.offset)
                      + sizeof(m.size_bytes)
                      + sizeof(m.block_id);
    }

    SSTFooter footer;
    footer.num_blocks = static_cast<uint32_t>(meta_.size());
    footer.meta_offset = meta_offset;
    footer.min_key = global_min_;
    footer.max_key = global_max_;
    footer.layout_type = ROW_LAYOUT;

    LOG_INFO("Writing footer: num_blocks={}, min_key={}, max_key={}, meta_offset={}",
            footer.num_blocks,
            footer.min_key,
            footer.max_key,
            footer.meta_offset);

    writer_.write_u32(footer.magic);
    writer_.write_u32(footer.num_blocks);
    writer_.write_u64(footer.meta_offset);
    writer_.write_u64(static_cast<uint64_t>(footer.min_key));
    writer_.write_u64(static_cast<uint64_t>(footer.max_key));
    writer_.write_u8(footer.layout_type);

    file_offset_ += sizeof(footer.magic)
                  + sizeof(footer.num_blocks)
                  + sizeof(footer.meta_offset)
                  + sizeof(footer.min_key)
                  + sizeof(footer.max_key)
                  + sizeof(footer.layout_type);

    file_.flush();

    LOG_INFO("SSTableBuilder: finish done (total_bytes_written={})", file_offset_);

    finished_ = true;
}
