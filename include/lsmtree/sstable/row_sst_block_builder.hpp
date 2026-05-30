#pragma once // lsmtree/sstable/row_sst_block_builder.hpp

#include <cstdint>

#include "storage/api/types.hpp"
#include "storage/model/schema.hpp"

namespace htap::lsmtree::sstable {

struct RowBlockMeta {
    storage::Key min_key;
    storage::Key max_key;
    uint32_t row_count;
    uint64_t offset;        // это сам заполняет SSTableBuilder
    uint64_t size_bytes;
    uint32_t block_id;      // это сам заполняет SSTableBuilder
};

struct RowSSTBlockResult {
    std::vector<uint8_t> data;   // бинарный блок
    RowBlockMeta meta;
};

class RowSSTBlockBuilder {
public:
    explicit RowSSTBlockBuilder(const storage::Schema& schema);

    void add(const storage::Row& row); 

    // while (!block.full()) { block.add(...); }
    // if (block.full()) { .. = block.finish(); ...; }
    bool full() const;
    size_t size_bytes() const;

    RowSSTBlockResult finish(); // после этого сделается автоматический reset

    void reset();

private:
    void encode_row(const storage::Row& row);

private:
    const storage::Schema& schema_;

    std::vector<uint8_t> buffer_;

    storage::Key min_key_;
    storage::Key max_key_;

    uint32_t row_count_;

    bool full_;

    static constexpr size_t TARGET_BLOCK_SIZE_BYTES = 4 * 1024; // 4 КБ
};

} // namespace htap::lsmtree::sstable
