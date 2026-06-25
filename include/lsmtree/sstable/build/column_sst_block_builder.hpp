#pragma once // lsmtree/sstable/build/column_sst_block_builder.hpp

#include <cstddef>
#include <cstdint>
#include <vector>

#include "storage/api/types.hpp"
#include "storage/model/schema.hpp"
#include "storage/read/sstable/numeric_stats.hpp"

namespace htap::lsmtree::sstable {

struct ColumnBlockMeta {
    storage::Key min_key;
    storage::Key max_key;
    uint16_t column_id;     // это сам заполняет SSTableBuilder
    uint32_t values_count;
    uint64_t offset;        // это сам заполняет SSTableBuilder
    uint64_t size_bytes;
    uint32_t block_id;      // это сам заполняет SSTableBuilder
};

struct ColumnSSTBlockResult {
    std::vector<uint8_t> data;
    ColumnBlockMeta meta;
    std::vector<storage::read::sstable::NumericBlockStats> numeric_stats;
};

class ColumnSSTBlockBuilder {
public:
    ColumnSSTBlockBuilder(
        const storage::Column& column,
        uint16_t column_id,
        std::size_t target_block_size_bytes = 4 * 1024
    );

    void add(const storage::Row& row);

    bool full() const;
    size_t size_bytes() const;

    ColumnSSTBlockResult finish();
    void reset();

private:
    void encode_value(const storage::NullableValue& value);
    void reset_numeric_stats();
    void update_numeric_stats(const storage::NullableValue& value);

private:
    const storage::Column& column_;
    uint16_t column_id_;

    // encoded block:
    // [null_bitmap][data]
    std::vector<uint8_t> buffer_;

    // temporary bitmap while building block
    std::vector<uint8_t> null_bitmap_;

    storage::Key min_key_;
    storage::Key max_key_;

    uint32_t values_count_;

    bool full_;

    std::vector<storage::read::sstable::NumericBlockStats> numeric_stats_;
    std::size_t target_block_size_bytes_;
};

} // namespace htap::lsmtree::sstable
