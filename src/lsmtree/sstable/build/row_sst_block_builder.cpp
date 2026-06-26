#include "lsmtree/sstable/build/row_sst_block_builder.hpp"

#include <cstring>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace htap::lsmtree::sstable;
using namespace htap::storage;

RowSSTBlockBuilder::RowSSTBlockBuilder(
    const Schema& schema,
    std::size_t target_block_size_bytes
)
    : schema_(schema)
    , buffer_()
    , target_block_size_bytes_(target_block_size_bytes)
{
    if (target_block_size_bytes_ == 0) {
        throw std::runtime_error("RowSSTBlockBuilder: target block size must be positive");
    }

    reset();
}

void RowSSTBlockBuilder::reset() {
    buffer_.clear();

    min_key_ = std::numeric_limits<Key>::max();
    max_key_ = std::numeric_limits<Key>::min();

    row_count_ = 0;
    full_ = false;
    reset_numeric_stats();
}

bool RowSSTBlockBuilder::full() const {
    return full_;
}

size_t RowSSTBlockBuilder::size_bytes() const {
    return buffer_.size();
}

RowSSTBlockResult RowSSTBlockBuilder::finish() {
    if (row_count_ == 0)
        throw std::runtime_error("Cannot finish empty SST block");

    RowBlockMeta meta{
        .min_key = min_key_,
        .max_key = max_key_,
        .row_count = row_count_,
        .offset = 0,        // позже заполнит в SSTableBuilder
        .size_bytes = buffer_.size(),
        .block_id = 0       // тоже заполнит SSTableBuilder
    };

    RowSSTBlockResult result{
        .data = std::move(buffer_),
        .meta = meta,
        .numeric_stats = std::move(numeric_stats_)
    };

    reset();
    return result;
}

// ADDING ROW

void RowSSTBlockBuilder::add(const Row& row) {
    if (full_)
        throw std::runtime_error("Block is already full");

    const Key key = std::get<Key>(*row[KEY_COLUMN_INDEX]);

    min_key_ = std::min(key, min_key_);
    max_key_ = std::max(key, max_key_);

    encode_row(row);
    update_numeric_stats(row);
    row_count_++;

    if (buffer_.size() >= target_block_size_bytes_)
        full_ = true;
}

// ROW ENCODING

namespace {

template<typename T>
void write_pod(std::vector<uint8_t>& buf, const T& value) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
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
            write_pod(buf, std::get<int64_t>(*val));
            break;
        }

        case ValueType::DOUBLE: {
            write_pod(buf, std::get<double>(*val));
            break;
        }

        case ValueType::STRING: {
            write_string(buf, std::get<std::string>(*val));
            break;
        }
    }
}

} // namespace

void RowSSTBlockBuilder::encode_row(const Row& row) {
    const Key key = std::get<Key>(*row[KEY_COLUMN_INDEX]);

    // key
    write_pod(buffer_, key);

    // bitmap only for non-key columns
    const size_t value_columns = schema_.size() - 1;

    const size_t bitmap_size = (value_columns + 7) / 8;

    std::vector<uint8_t> null_bitmap(bitmap_size, 0);

    for (size_t i = 1; i < schema_.size(); ++i) {
        const size_t logical_idx = i - 1;

        const size_t byte_idx = logical_idx / 8;
        const size_t bit_idx = logical_idx % 8;

        if (!row[i].has_value())
            null_bitmap[byte_idx] |= static_cast<uint8_t>(1u << bit_idx);
    }

    buffer_.insert(buffer_.end(), null_bitmap.begin(), null_bitmap.end());

    // values
    for (size_t i = 1; i < schema_.size(); ++i) {
        write_value_by_type(buffer_, row[i], schema_.get_column(i).type);
    }
}

void RowSSTBlockBuilder::reset_numeric_stats() {
    numeric_stats_.clear();

    for (std::size_t column_idx = 0; column_idx < schema_.size(); ++column_idx) {
        const auto& column = schema_.get_column(column_idx);

        if (column.is_key || !storage::read::sstable::is_numeric_type(column.type)) {
            continue;
        }

        storage::read::sstable::NumericStatsValue zero = std::int64_t{0};
        if (column.type == ValueType::DOUBLE) {
            zero = 0.0;
        }

        numeric_stats_.push_back(storage::read::sstable::NumericBlockStats{
            .column_idx = column_idx,
            .type = column.type,
            .has_value = false,
            .min_value = zero,
            .max_value = zero
        });
    }
}

void RowSSTBlockBuilder::update_numeric_stats(const Row& row) {
    for (auto& stats : numeric_stats_) {
        if (stats.column_idx >= row.size()) {
            throw std::runtime_error("RowSSTBlockBuilder: row size mismatch schema");
        }

        const auto& value = row[stats.column_idx];
        if (!value.has_value()) {
            continue;
        }

        if (stats.type == ValueType::INT64) {
            const auto current = std::get<std::int64_t>(*value);

            if (!stats.has_value) {
                stats.min_value = current;
                stats.max_value = current;
                stats.has_value = true;
                continue;
            }

            stats.min_value = std::min(std::get<std::int64_t>(stats.min_value), current);
            stats.max_value = std::max(std::get<std::int64_t>(stats.max_value), current);
            continue;
        }

        if (stats.type == ValueType::DOUBLE) {
            const auto current = std::get<double>(*value);
            if (std::isnan(current)) {
                continue;
            }

            if (!stats.has_value) {
                stats.min_value = current;
                stats.max_value = current;
                stats.has_value = true;
                continue;
            }

            stats.min_value = std::min(std::get<double>(stats.min_value), current);
            stats.max_value = std::max(std::get<double>(stats.max_value), current);
        }
    }
}
