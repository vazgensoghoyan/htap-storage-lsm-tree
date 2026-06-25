#pragma once // lsmtree/sstable/build/column_sstable_builder.hpp

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "lsmtree/sstable/build/sstable_build_result.hpp"
#include "storage/api/types.hpp"
#include "storage/model/schema.hpp"

#include "lsmtree/sstable/build/column_sst_block_builder.hpp"
#include "lsmtree/sstable/sstable_paths.hpp"

#include "utils/binary_writer.hpp"

namespace htap::lsmtree::sstable {

/**
 *  data.sst:
 *    [KeyBlock_0][ColBlock_1_0]...[ColBlock_(C-1)_0]   <- logical block 0
 *    [KeyBlock_1][ColBlock_1_1]...[ColBlock_(C-1)_1]   <- logical block 1
 *    ...
 *
 * Формат KeyBlock (column_idx=0):
 *   packed int64[] — без null-bitmap (ключ always not null INT64)
 *
 * Формат value ColBlock (column_idx=1..N-1):
 *   [null_bitmap][encoded values...] — стандартный ColumnSSTBlockBuilder
 */
class ColumnSSTableBuilder {
public:
    ColumnSSTableBuilder(
        const storage::Schema& schema,
        const std::filesystem::path& sstable_dir,
        uint32_t sparse_index_step = 1000
    );

    void add(const storage::Row& row);

    SSTableBuildResult finish();

private:
    void flush_block();

    void write_block_meta(
        storage::Key min_key,
        storage::Key max_key,
        uint64_t offset,
        uint64_t size_bytes,
        uint32_t values_count,
        uint32_t block_id,
        uint32_t column_idx
    );

    void write_info_file();

    bool any_col_builder_full() const;

private:
    const storage::Schema& schema_;

    SSTablePaths paths_;

    std::ofstream data_file_;
    std::ofstream meta_file_;
    std::ofstream index_file_;

    utils::BinaryWriter data_writer_;
    utils::BinaryWriter meta_writer_;
    utils::BinaryWriter index_writer_;
    
    std::vector<ColumnSSTBlockBuilder> col_builders_; // builders для value-колонок (schema index 1..N-1)
    std::vector<storage::Key> key_buffer_;            // буфер ключей текущего logical block

    uint32_t sparse_index_step_;
    uint64_t data_offset_ = 0;
    uint32_t block_id_ = 0;

    storage::Key global_min_;
    storage::Key global_max_;
    storage::Key last_key_;

    bool finished_ = false;
    bool first_row_ = true;

    // Сколько строк накапливать в одном logical block (для фиксированных типов).
    // Для переменных типов (STRING) flush срабатывает раньше через any_col_builder_full().
    static constexpr size_t TARGET_BLOCK_ROWS = 128;
};

} // namespace htap::lsmtree::sstable
