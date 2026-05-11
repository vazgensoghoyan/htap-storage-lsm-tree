#include "lsmtree/sstable/column_sst_block_builder.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

using namespace htap::lsmtree;
using namespace htap::storage;

ColumnSSTBlockBuilder::ColumnSSTBlockBuilder(const Column& column, int16_t column_id) : column_(column), column_id_(column_id) {
    reset();
}

void ColumnSSTBlockBuilder::reset() {
    buffer_.clear();
    null_bitmap_.clear();

    min_key_ = std::numeric_limits<int64_t>::max();
    max_key_ = std::numeric_limits<int64_t>::min();

    values_count_ = 0;
    full_ = false;
}

bool ColumnSSTBlockBuilder::full() const {
    return full_;
}

size_t ColumnSSTBlockBuilder::size_bytes() const {
    return buffer_.size();
}

void ColumnSSTBlockBuilder::add(const Row& row) {
    if (full_)
        throw std::runtime_error("Column block already full");

    Key key = std::get<Key>(*row[KEY_COLUMN_INDEX]);

    min_key_ = std::min(min_key_, key);
    max_key_ = std::max(max_key_, key);

    size_t row_index = values_count_;

    // expand bitmap if needed
    size_t byte_idx = row_index / 8;
    size_t bit_idx  = row_index % 8;

    if (null_bitmap_.size() <= byte_idx)
        null_bitmap_.resize(byte_idx + 1, 0);

    const auto& val = row[column_id_];

    if (!val.has_value()) {
        null_bitmap_[byte_idx] |= (1 << bit_idx);
    } else {
        encode_value(val);
    }

    values_count_++;

    // check size
    if (buffer_.size() >= TARGET_BLOCK_SIZE_BYTES)
        full_ = true;
}

void ColumnSSTBlockBuilder::encode_value(const NullableValue& value) {
    if (!value.has_value())
        return;

    switch (column_.type) {

        case ValueType::INT64: {
            int64_t v = std::get<int64_t>(*value);

            uint8_t bytes[sizeof(int64_t)];
            std::memcpy(bytes, &v, sizeof(int64_t));

            buffer_.insert(buffer_.end(), bytes, bytes + sizeof(int64_t));
            break;
        }

        case ValueType::DOUBLE: {
            double v = std::get<double>(*value);

            uint8_t bytes[sizeof(double)];
            std::memcpy(bytes, &v, sizeof(double));

            buffer_.insert(buffer_.end(), bytes, bytes + sizeof(double));
            break;
        }

        case ValueType::STRING: {
            const std::string& s = std::get<std::string>(*value);

            uint32_t len = static_cast<uint32_t>(s.size());

            uint8_t len_bytes[sizeof(uint32_t)];
            std::memcpy(len_bytes, &len, sizeof(uint32_t));

            buffer_.insert(buffer_.end(), len_bytes, len_bytes + sizeof(uint32_t));
            buffer_.insert(buffer_.end(), s.begin(), s.end());

            break;
        }
    }
}

ColumnSSTBlockResult ColumnSSTBlockBuilder::finish() {
    ColumnBlockMeta meta {
        .min_key = min_key_,
        .max_key = max_key_,
        .column_id = column_id_,
        .values_count = values_count_,
        .offset = 0,
        .size_bytes = buffer_.size() + null_bitmap_.size(),
        .block_id = 0
    };

    std::vector<uint8_t> out;
    out.reserve(meta.size_bytes);

    // 1. bitmap
    out.insert(out.end(), null_bitmap_.begin(), null_bitmap_.end());

    // 2. data
    out.insert(out.end(), buffer_.begin(), buffer_.end());

    reset();

    return ColumnSSTBlockResult {
        .data = std::move(out),
        .meta = meta
    };
}
