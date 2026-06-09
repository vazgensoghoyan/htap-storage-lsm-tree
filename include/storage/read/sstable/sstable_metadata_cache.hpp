#pragma once

#include "lsmtree/sstable/metadata/sstable_info.hpp"
#include "lsmtree/sstable/sparse_index/sparse_index_entry.hpp"
#include "storage/read/sstable/column_block_meta.hpp"
#include "storage/read/sstable/row_block_meta.hpp"
#include "storage/read/sstable/sstable_reader.hpp"

#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

namespace htap::storage::read::sstable {

class SSTableMetadataCache final {
public:
    SSTableMetadataCache(
        const lsmtree::SSTableInfo& info,
        std::uint32_t columns_count,
        std::uint32_t page_blocks = 256,
        std::size_t max_cached_pages = 64
    );

    const std::vector<lsmtree::sstable::SparseIndexEntry>& sparse_index() const noexcept;

    std::vector<RowBlockMeta> read_row_metadata_range(
        std::uint32_t first_block_id,
        std::uint32_t block_count
    );

    std::vector<ColumnBlockMeta> read_column_metadata_range(
        std::uint32_t first_block_id,
        std::uint32_t block_count
    );

private:
    struct RowPage {
        std::vector<RowBlockMeta> blocks;
        std::list<std::uint32_t>::iterator lru_it;
    };

    struct ColumnPage {
        std::vector<ColumnBlockMeta> blocks;
        std::list<std::uint32_t>::iterator lru_it;
    };

    std::vector<RowBlockMeta>& get_row_page(std::uint32_t page_id);
    std::vector<ColumnBlockMeta>& get_column_page(std::uint32_t page_id);

    void touch_row_page(std::uint32_t page_id, RowPage& page);
    void touch_column_page(std::uint32_t page_id, ColumnPage& page);

    void evict_row_if_needed();
    void evict_column_if_needed();

    std::uint32_t page_first_block(std::uint32_t page_id) const;
    std::uint32_t page_block_count(std::uint32_t page_id) const;

private:
    lsmtree::SSTableInfo info_;
    std::uint32_t columns_count_;
    std::uint32_t page_blocks_;
    std::size_t max_cached_pages_;

    SSTableReader reader_;
    std::vector<lsmtree::sstable::SparseIndexEntry> sparse_index_;

    std::list<std::uint32_t> row_lru_;
    std::unordered_map<std::uint32_t, RowPage> row_pages_;

    std::list<std::uint32_t> column_lru_;
    std::unordered_map<std::uint32_t, ColumnPage> column_pages_;
};

} 
