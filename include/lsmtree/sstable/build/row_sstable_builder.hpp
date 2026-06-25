#pragma once // lsmtree/sstable/build/sstable_builder.hpp

#include <filesystem>
#include <fstream>

#include "lsmtree/sstable/build/sstable_build_result.hpp"
#include "storage/api/types.hpp"
#include "storage/model/schema.hpp"
#include "storage/read/sstable/numeric_stats.hpp"

#include "lsmtree/sstable/build/row_sst_block_builder.hpp"
#include "lsmtree/sstable/sstable_paths.hpp"

#include "utils/binary_writer.hpp"

namespace htap::lsmtree::sstable {

class RowSSTableBuilder {
public:
    RowSSTableBuilder(
        const storage::Schema& schema,
        const std::filesystem::path& sstable_dir,
        uint32_t sparse_index_step = 1000 // TODO
    );

    void add(const storage::Row& row);

    SSTableBuildResult finish();

private:
    void flush_block();

    void write_info_file();
    void write_stats_file();

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
    std::vector<std::vector<storage::read::sstable::NumericBlockStats>> block_numeric_stats_;

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
