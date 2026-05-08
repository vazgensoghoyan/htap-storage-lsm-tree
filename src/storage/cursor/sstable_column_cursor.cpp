#include "storage/cursor/sstable_column_cursor.hpp"

#include <algorithm>
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

std::vector<Key> decode_key_block(
    std::vector<char> block_data,
    std::uint32_t values_count
) {
    std::size_t pos = 0;
    std::vector<Key> keys;
    keys.reserve(values_count);

    for (std::size_t key_idx = 0; key_idx < values_count; ++key_idx) {
        if (pos + sizeof(Key) > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable column block");
        }

        Key key;
        std::memcpy(&key, block_data.data() + pos, sizeof(Key));
        pos += sizeof(Key);

        keys.push_back(key);
    }

    return keys;
}

std::vector<NullableValue> decode_value_column_block(
    std::vector<char> block_data,
    std::uint32_t values_count,
    ValueType type
) {

    std::size_t pos = 0;

    auto read_bytes = [&](std::size_t size) {
        if (pos + size > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable column block");
        }

        std::vector<std::uint8_t> result(size);
        std::memcpy(result.data(), block_data.data() + pos, size);
        pos += size;

        return result;
    };

    auto read_string = [&]() {
        if (pos + sizeof(std::uint32_t) > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable column block");
        }

        std::uint32_t size;
        std::memcpy(&size, block_data.data() + pos, sizeof(std::uint32_t));
        pos += sizeof(std::uint32_t);

        if (pos + size > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable column block");
        }

        std::string result(block_data.data() + pos, block_data.data() + pos + size);
        pos += size;

        return result;
    };

    auto read_int64 = [&]() {
        if (pos + sizeof(std::int64_t) > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable column block");
        }

        std::int64_t value;
        std::memcpy(&value, block_data.data() + pos, sizeof(std::int64_t));
        pos += sizeof(std::int64_t);

        return value;
    };

    auto read_double = [&]() {
        if (pos + sizeof(double) > block_data.size()) {
            throw std::runtime_error("Unexpected end of SSTable column block");
        }

        double value;
        std::memcpy(&value, block_data.data() + pos, sizeof(double));
        pos += sizeof(double);

        return value;
    };


    std::vector<NullableValue> values;
    values.reserve(values_count);

    const std::size_t bitmap_size = (values_count + 7) / 8;
    const auto is_null_bitmap = read_bytes(bitmap_size);

    for (std::size_t value_idx = 0; value_idx < values_count; ++value_idx) {
        const std::size_t byte_idx = value_idx / 8;
        const std::size_t bit_idx = value_idx % 8;

        if (is_null_bitmap[byte_idx] & (1u << bit_idx)) {
            values.push_back(std::nullopt);
            continue;
        }

        switch (type) {
            case ValueType::INT64:
                values.push_back(read_int64());
                break;

            case ValueType::DOUBLE:
                values.push_back(read_double());
                break;

            case ValueType::STRING:
                values.push_back(read_string());
                break;
        }
    }

    return values;
}

}



SSTableColumnCursor::SSTableColumnCursor(
        std::filesystem::path path,
        std::vector<read::sstable::ColumnBlockMeta> blocks,
        read::sstable::KeyRange range,
        std::vector<ValueType> schema,
        std::vector<std::size_t> projection
) : reader_(std::move(path)), 
    blocks_(std::move(blocks)),
    range_(range),
    schema_(std::move(schema)),
    projection_(std::move(projection)) {
    
    if (schema_.empty()) {
        throw std::invalid_argument("SSTableColumnCursor requires non-empty schema");
    }

    if (KEY_COLUMN_INDEX >= schema_.size()) {
        throw std::invalid_argument("Schema does not contain key column");
    }

    if (schema_[KEY_COLUMN_INDEX] != ValueType::INT64) {
        throw std::invalid_argument("Key column must have INT64 type");
    }

    current_columns_.resize(schema_.size());

    load_next_non_empty_block();
}

bool SSTableColumnCursor::valid() const {
    return current_row_idx_ < current_keys_.size();
}

void SSTableColumnCursor::next() {
    if (!valid()) return;

    ++current_row_idx_;

    while (valid() && !read::sstable::contains(range_, key())) {
        ++current_row_idx_;
    }

    if (!valid()) {
        load_next_non_empty_block();
    }

}

Key SSTableColumnCursor::key() const {
    if (!valid()) {
        throw std::logic_error("SSTableColumnCursor::key() called on invalid cursor");
    }

    return current_keys_[current_row_idx_];
}

NullableValue SSTableColumnCursor::value(std::size_t column_idx) const {
    if (!valid()) {
        throw std::logic_error("SSTableColumnCursor::value() called on invalid cursor");
    }

    if (column_idx >= schema_.size()) {
        throw std::out_of_range("Column index out of schema range");
    }

    if (column_idx == KEY_COLUMN_INDEX) {
        return current_keys_[current_row_idx_];
    }

    const auto& column = current_columns_[column_idx];

    if (column.empty()) {
        throw std::logic_error("Column is not loaded");
    }

    if (current_row_idx_ >= column.size()) {
        throw std::out_of_range("Column value index out of range");
    }

    return column[current_row_idx_];

}

void SSTableColumnCursor::load_next_non_empty_block() {
    current_keys_.clear();
    
    for (auto& column : current_columns_) {
        column.clear();
    }

    current_row_idx_ = 0;

    while (next_block_idx_ < blocks_.size()) {
        std::size_t curr_block_id = blocks_[next_block_idx_].block_id;

        while (next_block_idx_ < blocks_.size() && curr_block_id == blocks_[next_block_idx_].block_id) {
            auto& block = blocks_[next_block_idx_++];

            if (block.column_idx >= schema_.size()) {
                throw std::runtime_error("Column block index is out of schema range");
            }

            if (block.column_idx == KEY_COLUMN_INDEX) {
                current_keys_ = decode_key_block(
                    reader_.read_block(block),
                    block.values_count
                );
                continue;
            }

            if (std::find(projection_.begin(), projection_.end(), block.column_idx) == projection_.end()) {
                continue;
            }

            current_columns_[block.column_idx] = decode_value_column_block(
                reader_.read_block(block),
                block.values_count,
                schema_[block.column_idx]
            );

        }

        while (valid() && !read::sstable::contains(range_, key())) {
            ++current_row_idx_;
        }

        if (valid()) {
            return;
        }

        current_keys_.clear();

        for (auto& column : current_columns_) {
            column.clear();
        }

        current_row_idx_ = 0;

    }
}

}