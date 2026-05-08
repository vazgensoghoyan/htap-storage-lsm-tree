#include "storage/cursor/sstable_row_cursor.hpp"

#include <stdexcept>
#include <cstddef>
#include <variant>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

namespace htap::storage::cursor {


namespace {

std::vector<Row> decode_row_block(
    std::vector<char> block_data,
    std::uint32_t row_count,
    const std::vector<ValueType>& schema
) {

    std::size_t pos = 0;

    auto read_bytes = [&](std::size_t size) {
        if (pos + size > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable row block");
        }

        std::vector<std::uint8_t> result(size);
        std::memcpy(result.data(), block_data.data() + pos, size);
        pos += size;

        return result;
    };

    auto read_string = [&]() {
        if (pos + sizeof(std::uint32_t) > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable row block");
        }

        std::uint32_t size;
        std::memcpy(&size, block_data.data() + pos, sizeof(std::uint32_t));
        pos += sizeof(std::uint32_t);

        if (pos + size > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable row block");
        }

        std::string result(block_data.data() + pos, block_data.data() + pos + size);
        pos += size;

        return result;
    };

    auto read_int64 = [&]() {
        if (pos + sizeof(std::int64_t) > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable row block");
        }

        std::int64_t value;
        std::memcpy(&value, block_data.data() + pos, sizeof(std::int64_t));
        pos += sizeof(std::int64_t);

        return value;
    };

    auto read_double = [&]() {
        if (pos + sizeof(double) > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable row block");
        }

        double value;
        std::memcpy(&value, block_data.data() + pos, sizeof(double));
        pos += sizeof(double);

        return value;
    };


    std::vector<Row> rows;
    rows.reserve(row_count);

    const std::size_t value_columns_count = schema.size() - 1;
    const std::size_t bitmap_size = (value_columns_count + 7) / 8;

    for (std::size_t row_idx = 0; row_idx < row_count; ++row_idx) {
        Row row(schema.size());

        const Key key = read_int64();
        row[KEY_COLUMN_INDEX] = key;

        const auto is_null_bitmap = read_bytes(bitmap_size);

        for (std::size_t column_idx = 1; column_idx < schema.size(); ++column_idx) {

            const std::size_t value_column_idx = column_idx - 1;

            const std::size_t byte_idx = value_column_idx / 8;
            const std::size_t bit_idx = value_column_idx % 8;

            if (is_null_bitmap[byte_idx] & (1u << bit_idx)) {
                row[column_idx] = std::nullopt;
                continue;
            }

            switch (schema[column_idx]) {
                case ValueType::INT64:
                    row[column_idx] = read_int64();
                    break;

                case ValueType::DOUBLE:
                    row[column_idx] = read_double();
                    break;

                case ValueType::STRING:
                    row[column_idx] = read_string();
                    break;
            }

        }

        rows.push_back(std::move(row));

    }

    return rows;
}

}



SSTableRowCursor::SSTableRowCursor(
        std::filesystem::path path,
        std::vector<read::sstable::BlockMeta> blocks,
        read::sstable::KeyRange range,
        std::vector<ValueType> schema,
        std::vector<std::size_t> projection
) : reader_(std::move(path)), 
    blocks_(std::move(blocks)),
    range_(range),
    schema_(std::move(schema)),
    projection_(std::move(projection)) {
    
    if (schema_.empty()) {
        throw std::invalid_argument("SSTableRowCursor requires non-empty schema");
    }

    if (KEY_COLUMN_INDEX >= schema_.size()) {
        throw std::invalid_argument("Schema does not contain key column");
    }

    if (schema_[KEY_COLUMN_INDEX] != ValueType::INT64) {
        throw std::invalid_argument("Key column must have INT64 type");
    }

    load_next_non_empty_block();
}

bool SSTableRowCursor::valid() const {
    return current_row_idx_ < current_rows_.size();
}

void SSTableRowCursor::next() {
    if (!valid()) return;

    ++current_row_idx_;

    while (valid() && !read::sstable::contains(range_, key())) {
        ++current_row_idx_;
    }

    if (!valid()) {
        load_next_non_empty_block();
    }

}

Key SSTableRowCursor::key() const {
    if (!valid()) {
        throw std::logic_error("SSTableRowCursor::key() called on invalid cursor");
    }

    const Row& row = current_rows_[current_row_idx_];

    return std::get<Key>(*row[KEY_COLUMN_INDEX]);
}

NullableValue SSTableRowCursor::value(std::size_t column_idx) const {
    if (!valid()) {
        throw std::logic_error("SSTableRowCursor::value() called on invalid cursor");
    }

    if (column_idx >= schema_.size()) {
        throw std::out_of_range("Column index out of schema range");
    }

    const Row& row = current_rows_[current_row_idx_];

    if (column_idx >= row.size()) {
        throw std::out_of_range("Column index out of row range");
    }

    return row[column_idx];
}

void SSTableRowCursor::load_next_non_empty_block() {
    current_rows_.clear();
    current_row_idx_ = 0;

    while (next_block_idx_ < blocks_.size())
    {
        const auto& block = blocks_[next_block_idx_++];
        auto block_data = reader_.read_block(block);

        current_rows_ = decode_row_block(std::move(block_data), block.row_count, schema_);

        current_row_idx_ = 0;

        while (valid() && !read::sstable::contains(range_, key())) {
            ++current_row_idx_;
        }

        if (valid()) {
            return;
        }

        current_rows_.clear();
        current_row_idx_ = 0;

    }
}

}