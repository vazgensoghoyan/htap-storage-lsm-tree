#include "storage/read/sstable/sstable_block_cache.hpp"

#include <functional>
#include <stdexcept>
#include <utility>

namespace htap::storage::read::sstable {

std::size_t SSTableBlockCache::CacheKeyHash::operator()(const CacheKey& key) const noexcept {
    std::size_t seed = std::hash<std::uint64_t>{}(key.sstable_id);
    seed ^= std::hash<int>{}(static_cast<int>(key.kind)) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    seed ^= std::hash<std::size_t>{}(key.block_id) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    seed ^= std::hash<std::size_t>{}(key.column_idx) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    return seed;
}

SSTableBlockCache::SSTableBlockCache(std::size_t max_cached_bytes)
    : max_cached_bytes_(max_cached_bytes) {
    if (max_cached_bytes_ == 0) {
        throw std::runtime_error("SSTableBlockCache max_cached_bytes must be positive");
    }
}

std::vector<char> SSTableBlockCache::read_row_block(
    std::uint64_t sstable_id,
    const RowBlockMeta& block,
    const std::function<std::vector<char>()>& loader
) {
    return read_block(
        CacheKey{
            .sstable_id = sstable_id,
            .kind = BlockKind::Row,
            .block_id = block.block_id,
            .column_idx = 0
        },
        loader
    );
}

std::vector<char> SSTableBlockCache::read_column_block(
    std::uint64_t sstable_id,
    const ColumnBlockMeta& block,
    const std::function<std::vector<char>()>& loader
) {
    return read_block(
        CacheKey{
            .sstable_id = sstable_id,
            .kind = BlockKind::Column,
            .block_id = block.block_id,
            .column_idx = block.column_idx
        },
        loader
    );
}

SSTableBlockCache::Stats SSTableBlockCache::stats() const noexcept {
    return Stats{
        .hits = hits_,
        .misses = misses_,
        .cached_bytes = cached_bytes_,
        .cached_blocks = entries_.size()
    };
}

std::vector<char> SSTableBlockCache::read_block(
    const CacheKey& key,
    const std::function<std::vector<char>()>& loader
) {
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        ++hits_;
        touch(it->second);
        return it->second.bytes;
    }

    ++misses_;
    auto bytes = loader();
    const auto size = bytes.size();

    std::list<CacheKey>::iterator lru_it;
    insert_lru_front(key, lru_it);

    auto inserted_it = entries_.emplace(
        key,
        CacheEntry{
            .bytes = std::move(bytes),
            .lru_it = lru_it,
            .size = size
        }
    ).first;

    cached_bytes_ += size;
    evict_if_needed();

    return inserted_it->second.bytes;
}

void SSTableBlockCache::touch(CacheEntry& entry) {
    lru_.splice(lru_.begin(), lru_, entry.lru_it);
    entry.lru_it = lru_.begin();
}

void SSTableBlockCache::insert_lru_front(
    const CacheKey& key,
    std::list<CacheKey>::iterator& lru_it
) {
    lru_.push_front(key);
    lru_it = lru_.begin();
}

void SSTableBlockCache::evict_page(const CacheKey& key) {
    auto it = entries_.find(key);
    if (it == entries_.end()) {
        return;
    }

    cached_bytes_ -= it->second.size;
    entries_.erase(it);
}

void SSTableBlockCache::evict_if_needed() {
    while (cached_bytes_ > max_cached_bytes_ && lru_.size() > 1) {
        const auto victim = lru_.back();
        lru_.pop_back();
        evict_page(victim);
    }
}

}
