#pragma once // lsmtree/sstable/sstable_builder.hpp

#include <filesystem>
#include <fstream>

#include "storage/api/types.hpp"
#include "storage/model/schema.hpp"

#include "lsmtree/sstable/row_sst_block_builder.hpp"
#include "lsmtree/sstable/sstable_paths.hpp"

#include "utils/binary_writer.hpp"

namespace htap::lsmtree::sstable {

class SSTableBuilder {
public:
    SSTableBuilder(
        const storage::Schema& schema,
        const std::filesystem::path& sstable_dir,
        uint32_t sparse_index_step
    );

    void add(const storage::Row& row);

    void finish();

private:
    void flush_block();

    void write_info_file();

private:
    const storage::Schema& schema_;

    SSTablePaths paths_;

    std::ofstream data_file_;
    std::ofstream meta_file_;
    std::ofstream index_file_;

    utils::BinaryWriter data_writer_;
    utils::BinaryWriter meta_writer_;
    utils::BinaryWriter index_writer_;

    RowSSTBlockBuilder block_builder_;

    uint32_t sparse_index_step_;

    uint64_t data_offset_ = 0;
    uint32_t block_id_ = 0;

    bool finished_ = false;

    storage::Key global_min_;
    storage::Key global_max_;

    storage::Key last_key_;
    bool first_row_ = true;
};

} // namespace htap::lsmtree::sstable
