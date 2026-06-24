#pragma once

#include "storage/read/sstable/column_block_meta.hpp"
#include "storage/read/sstable/row_block_meta.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <vector>

namespace htap::storage::read::sstable {

class SSTableBlockCache final {
public:
    struct Stats {
        std::uint64_t hits = 0;
        std::uint64_t misses = 0;
        std::size_t cached_bytes = 0;
        std::size_t cached_blocks = 0;
    };

    explicit SSTableBlockCache(std::size_t max_cached_bytes = 64 * 1024 * 1024);

    std::vector<char> read_row_block(
        std::uint64_t sstable_id,
        const RowBlockMeta& block,
        const std::function<std::vector<char>()>& loader
    );

    std::vector<char> read_column_block(
        std::uint64_t sstable_id,
        const ColumnBlockMeta& block,
        const std::function<std::vector<char>()>& loader
    );

    Stats stats() const noexcept;

private:
    enum class BlockKind {
        Row,
        Column
    };

    struct CacheKey {
        std::uint64_t sstable_id = 0;
        BlockKind kind = BlockKind::Row;
        std::size_t block_id = 0;
        std::size_t column_idx = 0;

        bool operator==(const CacheKey& other) const noexcept {
            return sstable_id == other.sstable_id &&
                kind == other.kind &&
                block_id == other.block_id &&
                column_idx == other.column_idx;
        }
    };

    struct CacheKeyHash {
        std::size_t operator()(const CacheKey& key) const noexcept;
    };

    struct CacheEntry {
        std::vector<char> bytes;
        std::list<CacheKey>::iterator lru_it;
        std::size_t size = 0;
    };

    std::vector<char> read_block(
        const CacheKey& key,
        const std::function<std::vector<char>()>& loader
    );

    void touch(CacheEntry& entry);
    void insert_lru_front(const CacheKey& key, std::list<CacheKey>::iterator& lru_it);
    void evict_if_needed();
    void evict_page(const CacheKey& key);

private:
    std::size_t max_cached_bytes_;
    std::size_t cached_bytes_ = 0;
    std::uint64_t hits_ = 0;
    std::uint64_t misses_ = 0;

    std::list<CacheKey> lru_;
    std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> entries_;
};

}
