#pragma once

#include "lsmtree/sstable/metadata/sstable_info.hpp"
#include "lsmtree/sstable/format/sparse_index_entry.hpp"
#include "storage/read/sstable/column_block_meta.hpp"
#include "storage/read/sstable/numeric_stats.hpp"
#include "storage/read/sstable/row_block_meta.hpp"
#include "storage/read/sstable/sstable_reader.hpp"

#include <cstdint>
#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

namespace htap::storage::read::sstable {

class SSTableMetadataCache final {
public:
    SSTableMetadataCache(
        const lsmtree::sstable::SSTableInfo& info,
        std::uint32_t columns_count,
        std::uint32_t page_blocks = 256,
        std::size_t max_cached_bytes = 4 * 1024 * 1024
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

    NumericStatsRange read_numeric_stats_range(
        std::uint32_t first_block_id,
        std::uint32_t block_count,
        const std::vector<std::size_t>& column_indices
    );

private:
    enum class PageKind {
        RowMetadata,
        ColumnMetadata,
        NumericStats
    };

    struct CacheKey {
        PageKind kind = PageKind::RowMetadata;
        std::uint32_t page_id = 0;
        std::size_t column_idx = 0;

        bool operator==(const CacheKey& other) const noexcept {
            return kind == other.kind &&
                page_id == other.page_id &&
                column_idx == other.column_idx;
        }
    };

    struct CacheKeyHash {
        std::size_t operator()(const CacheKey& key) const noexcept;
    };

    struct RowPage {
        std::vector<RowBlockMeta> blocks;
        std::list<CacheKey>::iterator lru_it;
        std::size_t bytes = 0;
    };

    struct ColumnPage {
        std::vector<ColumnBlockMeta> blocks;
        std::list<CacheKey>::iterator lru_it;
        std::size_t bytes = 0;
    };

    struct StatsPage {
        std::optional<std::vector<NumericBlockStats>> blocks;
        std::list<CacheKey>::iterator lru_it;
        std::size_t bytes = 0;
    };

    std::vector<RowBlockMeta>& get_row_page(std::uint32_t page_id);
    std::vector<ColumnBlockMeta>& get_column_page(std::uint32_t page_id);
    StatsPage& get_stats_page(std::size_t column_idx, std::uint32_t page_id);

    void touch_page(const CacheKey& key, std::list<CacheKey>::iterator& lru_it);

    void insert_lru_front(const CacheKey& key, std::list<CacheKey>::iterator& lru_it);
    void evict_if_needed();
    void evict_page(const CacheKey& key);

    std::uint32_t page_first_block(std::uint32_t page_id) const;
    std::uint32_t page_block_count(std::uint32_t page_id) const;

private:
    lsmtree::sstable::SSTableInfo info_;
    std::uint32_t columns_count_;
    std::uint32_t page_blocks_;
    std::size_t max_cached_bytes_;
    std::size_t cached_bytes_ = 0;

    SSTableReader reader_;
    std::vector<lsmtree::sstable::SparseIndexEntry> sparse_index_;

    std::list<CacheKey> lru_;
    std::unordered_map<std::uint32_t, RowPage> row_pages_;
    std::unordered_map<std::uint32_t, ColumnPage> column_pages_;
    std::unordered_map<CacheKey, StatsPage, CacheKeyHash> stats_pages_;
};

} 
