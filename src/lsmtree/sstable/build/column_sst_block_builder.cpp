#include "lsmtree/sstable/build/column_sst_block_builder.hpp"

#include <cstring>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace htap::lsmtree::sstable;
using namespace htap::storage;

ColumnSSTBlockBuilder::ColumnSSTBlockBuilder(const Column& column, uint16_t column_id)
    : column_(column)
    , column_id_(column_id)
{
    reset();
}

void ColumnSSTBlockBuilder::reset() {
    buffer_.clear();
    null_bitmap_.clear();

    min_key_ = std::numeric_limits<Key>::max();
    max_key_ = std::numeric_limits<Key>::min();

    values_count_ = 0;
    full_ = false;
    reset_numeric_stats();
}

bool ColumnSSTBlockBuilder::full() const {
    return full_;
}

size_t ColumnSSTBlockBuilder::size_bytes() const {
    return buffer_.size() + null_bitmap_.size();
}

void ColumnSSTBlockBuilder::add(const Row& row) {
    if (full_)
        throw std::runtime_error("Column block already full");

    if (column_id_ >= static_cast<uint16_t>(row.size()))
        throw std::runtime_error("ColumnSSTBlockBuilder: column_id out of range");

    const Key key = std::get<Key>(*row[KEY_COLUMN_INDEX]);

    min_key_ = std::min(min_key_, key);
    max_key_ = std::max(max_key_, key);

    const size_t row_index = values_count_;

    const size_t byte_idx = row_index / 8;
    const size_t bit_idx  = row_index % 8;

    if (null_bitmap_.size() <= byte_idx)
        null_bitmap_.resize(byte_idx + 1, 0);

    const auto& val = row[column_id_];

    if (!val.has_value()) {
        null_bitmap_[byte_idx] |= static_cast<uint8_t>(1u << bit_idx);
    } else {
        encode_value(val);
    }

    update_numeric_stats(val);

    values_count_++;

    if (size_bytes() >= TARGET_BLOCK_SIZE_BYTES)
        full_ = true;
}

void ColumnSSTBlockBuilder::encode_value(const NullableValue& value) {
    if (!value.has_value())
        return;

    switch (column_.type) {

        case ValueType::INT64: {
            const int64_t v = std::get<int64_t>(*value);

            uint8_t bytes[sizeof(int64_t)];
            std::memcpy(bytes, &v, sizeof(int64_t));

            buffer_.insert(buffer_.end(), bytes, bytes + sizeof(int64_t));
            break;
        }

        case ValueType::DOUBLE: {
            const double v = std::get<double>(*value);

            uint8_t bytes[sizeof(double)];
            std::memcpy(bytes, &v, sizeof(double));

            buffer_.insert(buffer_.end(), bytes, bytes + sizeof(double));
            break;
        }

        case ValueType::STRING: {
            const std::string& s = std::get<std::string>(*value);

            const uint32_t len = static_cast<uint32_t>(s.size());

            uint8_t len_bytes[sizeof(uint32_t)];
            std::memcpy(len_bytes, &len, sizeof(uint32_t));

            buffer_.insert(buffer_.end(), len_bytes, len_bytes + sizeof(uint32_t));
            buffer_.insert(buffer_.end(), s.begin(), s.end());
            break;
        }
    }
}

ColumnSSTBlockResult ColumnSSTBlockBuilder::finish() {
    if (values_count_ == 0)
        throw std::runtime_error("Cannot finish empty column block");

    ColumnBlockMeta meta{
        .min_key = min_key_,
        .max_key = max_key_,
        .column_id = static_cast<uint16_t>(column_id_),
        .values_count = values_count_,
        .offset = 0,
        .size_bytes = static_cast<uint64_t>(size_bytes()),
        .block_id = 0
    };

    std::vector<uint8_t> out;
    out.reserve(meta.size_bytes);

    // 1. bitmap
    out.insert(out.end(), null_bitmap_.begin(), null_bitmap_.end());

    // 2. data
    out.insert(out.end(), buffer_.begin(), buffer_.end());

    ColumnSSTBlockResult result{
        .data = std::move(out),
        .meta = meta,
        .numeric_stats = std::move(numeric_stats_)
    };

    reset();

    return result;
}

void ColumnSSTBlockBuilder::reset_numeric_stats() {
    numeric_stats_.clear();

    if (column_id_ == KEY_COLUMN_INDEX || !storage::read::sstable::is_numeric_type(column_.type)) {
        return;
    }

    storage::read::sstable::NumericStatsValue zero = std::int64_t{0};
    if (column_.type == ValueType::DOUBLE) {
        zero = 0.0;
    }

    numeric_stats_.push_back(storage::read::sstable::NumericBlockStats{
        .column_idx = column_id_,
        .type = column_.type,
        .has_value = false,
        .min_value = zero,
        .max_value = zero
    });
}

void ColumnSSTBlockBuilder::update_numeric_stats(const NullableValue& value) {
    if (numeric_stats_.empty() || !value.has_value()) {
        return;
    }

    auto& stats = numeric_stats_.front();

    if (stats.type == ValueType::INT64) {
        const auto current = std::get<std::int64_t>(*value);

        if (!stats.has_value) {
            stats.min_value = current;
            stats.max_value = current;
            stats.has_value = true;
            return;
        }

        stats.min_value = std::min(std::get<std::int64_t>(stats.min_value), current);
        stats.max_value = std::max(std::get<std::int64_t>(stats.max_value), current);
        return;
    }

    if (stats.type == ValueType::DOUBLE) {
        const auto current = std::get<double>(*value);
        if (std::isnan(current)) {
            return;
        }

        if (!stats.has_value) {
            stats.min_value = current;
            stats.max_value = current;
            stats.has_value = true;
            return;
        }

        stats.min_value = std::min(std::get<double>(stats.min_value), current);
        stats.max_value = std::max(std::get<double>(stats.max_value), current);
    }
}
