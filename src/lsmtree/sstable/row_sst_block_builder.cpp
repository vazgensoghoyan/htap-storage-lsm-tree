#include "lsmtree/sstable/row_sst_block_builder.hpp"

#include <limits>
#include <cstring>

using namespace htap::lsmtree;
using namespace htap::storage;

RowSSTBlockBuilder::RowSSTBlockBuilder(const Schema& schema) : schema_(schema), buffer_() {
    reset();
}

void RowSSTBlockBuilder::reset() {
    buffer_.clear();

    min_key_ = std::numeric_limits<int64_t>::max();
    max_key_ = std::numeric_limits<int64_t>::min();

    row_count_ = 0;

    full_ = false;
}

bool RowSSTBlockBuilder::full() const {
    return full_;
}

size_t RowSSTBlockBuilder::size_bytes() const {
    return buffer_.size();
}

// FINISHING

SSTBlockResult RowSSTBlockBuilder::finish() {
    RowBlockMeta meta {
        .min_key = min_key_,
        .max_key = max_key_,
        .row_count = row_count_,
        .offset = 0,        // позже заполнит в SSTableBuilder
        .size_bytes = buffer_.size(),
        .block_id = 0       // тоже заполнит SSTableBuilder
    };

    SSTBlockResult result {
        .data = std::move(buffer_),
        .meta = meta
    };

    reset();
    return result;
}

// ADDING ROW

void RowSSTBlockBuilder::add(const Row& row) {
    if (full_)
        throw std::runtime_error("Block is already full");

    Key key = std::get<Key>(*row[KEY_COLUMN_INDEX]);

    min_key_ = std::min(key, min_key_);
    max_key_ = std::max(key, max_key_);

    encode_row(row);
    row_count_++;

    if (buffer_.size() >= TARGET_BLOCK_SIZE_BYTES)
        full_ = true;
}

// ROW ENCODING

namespace {

template <typename T>
void write_pod(std::vector<uint8_t>& buf, T value) {
    uint8_t bytes[sizeof(T)];
    std::memcpy(bytes, &value, sizeof(T));
    buf.insert(buf.end(), bytes, bytes + sizeof(T));
}

void write_string(std::vector<uint8_t>& buf, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    write_pod(buf, len);
    buf.insert(buf.end(), s.begin(), s.end());
}

void write_value_by_type(std::vector<uint8_t>& buf, const NullableValue& val, ValueType type) {
    if (!val.has_value()) return;
    switch (type) {
        case ValueType::INT64: {
            auto v = std::get<int64_t>(*val);
            write_pod(buf, v);
            break;
        } case ValueType::DOUBLE: {
            auto v = std::get<double>(*val);
            write_pod(buf, v);
            break;
        } case ValueType::STRING: {
            auto v = std::get<std::string>(*val);
            write_string(buf, v);
            break;
        }
    }
}

} // namespace

void RowSSTBlockBuilder::encode_row(const Row& row) {
    Key key = std::get<Key>(*row[KEY_COLUMN_INDEX]);

    write_pod(buffer_, key);

    size_t col_count = schema_.size();
    size_t bitmap_size = (col_count + 7) / 8;

    std::vector<uint8_t> null_bitmap(bitmap_size, 0);

    for (size_t i = 1; i < col_count; ++i) {
        size_t byte_idx = i / 8;
        size_t bit_idx  = i % 8;

        if (!row[i].has_value()) {
            null_bitmap[byte_idx] |= (1 << bit_idx);
        }
    }

    buffer_.insert(buffer_.end(), null_bitmap.begin(), null_bitmap.end());

    for (size_t i = 1; i < col_count; ++i) {
        const auto& val = row[i];
        const auto& type = schema_.get_column(i).type;
        write_value_by_type(buffer_, val, type);
    }
}
