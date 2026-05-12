#pragma once // lsmtree/sstable/build/sstable_builder.hpp

#include <fstream>
#include <vector>
#include <string>

#include "storage/api/types.hpp"
#include "storage/model/schema.hpp"

#include "lsmtree/sstable/build/row_sst_block_builder.hpp"
#include "lsmtree/sstable/build/sst_footer.hpp"

#include "utils/binary_writer.hpp"

namespace htap::lsmtree {

struct SSTableBuildResult {
    storage::Key min_key;
    storage::Key max_key;

    uint64_t file_size_bytes;

    uint64_t meta_offset;

    uint32_t num_blocks;
};

class SSTableBuilder {
public:
    SSTableBuilder(const storage::Schema& schema, const std::string& path);

    void add(const storage::Row& row);

    SSTableBuildResult finish();

private:
    void flush_block();

private:
    const storage::Schema& schema_;

    std::ofstream file_;
    htap::utils::BinaryWriter writer_;

    RowSSTBlockBuilder block_builder_;
    std::vector<RowBlockMeta> meta_;

    uint64_t file_offset_ = 0;
    uint32_t block_id_ = 0;

    bool finished_ = false;

    storage::Key global_min_;
    storage::Key global_max_;

    storage::Key last_key_;
    bool first_row_ = true;
};

} // namespace htap::lsmtree
