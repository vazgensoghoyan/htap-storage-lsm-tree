#pragma once

#include "storage/api/cursor_interface.hpp"
#include "storage/api/types.hpp"
#include "storage/read/sstable/column_block_meta.hpp"
#include "storage/read/sstable/key_range.hpp"
#include "storage/read/sstable/sstable_reader.hpp"

#include <memory>
#include <vector>
#include <filesystem>
#include <cstddef>

namespace htap::storage::cursor {

class SSTableColumnCursor final : public ICursor {
public:
    SSTableColumnCursor(
        std::filesystem::path path,
        std::vector<read::sstable::ColumnBlockMeta> blocks,
        read::sstable::KeyRange range,
        std::vector<ValueType> schema,
        std::vector<std::size_t> projection
    );

    SSTableColumnCursor(
        std::unique_ptr<read::sstable::SSTableReader> reader,
        std::vector<read::sstable::ColumnBlockMeta> blocks,
        read::sstable::KeyRange range,
        std::vector<ValueType> schema,
        std::vector<std::size_t> projection
    );

    bool valid() const override;

    void next() override;

    Key key() const override;

    NullableValue value(std::size_t column_idx) const override;

private:
    void load_next_non_empty_block();

private:
    std::unique_ptr<read::sstable::SSTableReader> reader_;
    std::vector<read::sstable::ColumnBlockMeta> blocks_;
    read::sstable::KeyRange range_;
    std::vector<ValueType> schema_;
    std::vector<std::size_t> projection_;

    std::size_t next_block_idx_ = 0;
    std::size_t current_row_idx_ = 0;

    std::vector<Key> current_keys_;
    std::vector<std::vector<NullableValue>> current_columns_;
};

}
