#pragma once // lsmtree/sstable/column_sst_block_builder.hpp

#include <cstdint>
#include <vector>

#include "storage/api/types.hpp"
#include "storage/model/schema.hpp"

namespace htap::lsmtree {

struct ColumnBlockMeta {
    int64_t min_key;
    int64_t max_key;
    int16_t column_id;      // это сам заполняет SSTableBuilder
    uint32_t values_count;
    uint64_t offset;        // это сам заполняет SSTableBuilder
    uint64_t size_bytes;
    uint32_t block_id;      // это сам заполняет SSTableBuilder
};

struct ColumnSSTBlockResult {
    std::vector<uint8_t> data;
    ColumnBlockMeta meta;
};

class ColumnSSTBlockBuilder {
public:
    ColumnSSTBlockBuilder(const storage::Column& column, int16_t column_id);

    void add(const storage::Row& row);

    bool full() const;
    size_t size_bytes() const;

    ColumnSSTBlockResult finish();

    void reset();

private:
    void encode_value(const storage::NullableValue& value);

private:
    const storage::Column& column_;

    int16_t column_id_;

    // encoded block:
    // [null_bitmap][data]
    std::vector<uint8_t> buffer_;

    // temporary bitmap while building block
    std::vector<uint8_t> null_bitmap_;

    storage::Key min_key_;
    storage::Key max_key_;

    uint32_t values_count_;

    bool full_;

    static constexpr size_t TARGET_BLOCK_SIZE_BYTES = 4 * 1024; // 4 KB
};

} // namespace htap::lsmtree
