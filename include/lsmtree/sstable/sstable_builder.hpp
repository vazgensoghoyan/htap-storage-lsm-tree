#pragma once // lsmtree/sstable/sstable_builder.hpp

#include <fstream>
#include <vector>
#include <string>

#include "storage/api/types.hpp"
#include "storage/model/schema.hpp"

#include "lsmtree/sstable/row_sst_block_builder.hpp"
#include "lsmtree/sstable/sst_footer.hpp"

namespace htap::lsmtree {

class SSTableBuilder {
public:
    SSTableBuilder(const storage::Schema& schema, const std::string& path);

    void add(const storage::Row& row);

    void finish();

private:
    void flush_block();

private:
    const storage::Schema& schema_;

    std::ofstream file_;

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
