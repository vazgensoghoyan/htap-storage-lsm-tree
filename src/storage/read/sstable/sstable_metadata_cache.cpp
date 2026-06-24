#include "storage/read/sstable/sstable_metadata_cache.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>

namespace htap::storage::read::sstable {

namespace {

std::size_t row_page_bytes(const std::vector<RowBlockMeta>& blocks) {
    return blocks.size() * ROW_BLOCK_META_ON_DISK_SIZE;
}

std::size_t column_page_bytes(const std::vector<ColumnBlockMeta>& blocks) {
    return blocks.size() * COLUMN_BLOCK_META_ON_DISK_SIZE;
}

std::size_t stats_page_bytes(const std::optional<std::vector<NumericBlockStats>>& blocks) {
    if (!blocks.has_value()) {
        return 1;
    }

    return blocks->size() * 17;
}

}

std::size_t SSTableMetadataCache::CacheKeyHash::operator()(const CacheKey& key) const noexcept {
    std::size_t seed = std::hash<int>{}(static_cast<int>(key.kind));
    seed ^= std::hash<std::uint32_t>{}(key.page_id) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    seed ^= std::hash<std::size_t>{}(key.column_idx) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    return seed;
}

SSTableMetadataCache::SSTableMetadataCache(
    const lsmtree::sstable::SSTableInfo& info,
    std::uint32_t columns_count,
    std::uint32_t page_blocks,
    std::size_t max_cached_bytes
)
    : info_(info),
    columns_count_(columns_count),
    page_blocks_(page_blocks),
    max_cached_bytes_(max_cached_bytes),
    reader_(info.path),
    sparse_index_(reader_.read_sparse_index()) {

    if (page_blocks_ == 0) {
        throw std::runtime_error("SSTableMetadataCache page_blocks must be positive");
    }

    if (max_cached_bytes_ == 0) {
        throw std::runtime_error("SSTableMetadataCache max_cached_bytes must be positive");
    }
}

const std::vector<lsmtree::sstable::SparseIndexEntry>&
SSTableMetadataCache::sparse_index() const noexcept {
    return sparse_index_;
}

std::uint32_t SSTableMetadataCache::page_first_block(std::uint32_t page_id) const {
    return page_id * page_blocks_;
}

std::uint32_t SSTableMetadataCache::page_block_count(std::uint32_t page_id) const {
    const auto first = page_first_block(page_id);

    if (first >= info_.num_blocks) {
        return 0;
    }

    return std::min<std::uint32_t>(page_blocks_, info_.num_blocks - first);
}

void SSTableMetadataCache::touch_page(
    const CacheKey& key,
    std::list<CacheKey>::iterator& lru_it
) {
    (void)key;
    lru_.splice(lru_.begin(), lru_, lru_it);
    lru_it = lru_.begin();
}

void SSTableMetadataCache::insert_lru_front(
    const CacheKey& key,
    std::list<CacheKey>::iterator& lru_it
) {
    lru_.push_front(key);
    lru_it = lru_.begin();
}

void SSTableMetadataCache::evict_page(const CacheKey& key) {
    switch (key.kind) {
        case PageKind::RowMetadata: {
            auto it = row_pages_.find(key.page_id);
            if (it == row_pages_.end()) {
                return;
            }

            cached_bytes_ -= it->second.bytes;
            row_pages_.erase(it);
            return;
        }

        case PageKind::ColumnMetadata: {
            auto it = column_pages_.find(key.page_id);
            if (it == column_pages_.end()) {
                return;
            }

            cached_bytes_ -= it->second.bytes;
            column_pages_.erase(it);
            return;
        }

        case PageKind::NumericStats: {
            auto it = stats_pages_.find(key);
            if (it == stats_pages_.end()) {
                return;
            }

            cached_bytes_ -= it->second.bytes;
            stats_pages_.erase(it);
            return;
        }
    }
}

void SSTableMetadataCache::evict_if_needed() {
    while (cached_bytes_ > max_cached_bytes_ && lru_.size() > 1) {
        const auto victim = lru_.back();
        lru_.pop_back();
        evict_page(victim);
    }
}

std::vector<RowBlockMeta>& SSTableMetadataCache::get_row_page(std::uint32_t page_id) {
    auto it = row_pages_.find(page_id);
    if (it != row_pages_.end()) {
        touch_page(CacheKey{.kind = PageKind::RowMetadata, .page_id = page_id}, it->second.lru_it);
        return it->second.blocks;
    }

    const auto first_block = page_first_block(page_id);
    const auto block_count = page_block_count(page_id);

    auto blocks = reader_.read_row_metadata_range(first_block, block_count);
    const auto bytes = row_page_bytes(blocks);

    CacheKey key{
        .kind = PageKind::RowMetadata,
        .page_id = page_id,
        .column_idx = 0
    };

    std::list<CacheKey>::iterator lru_it;
    insert_lru_front(key, lru_it);

    auto inserted_it = row_pages_.emplace(
        page_id,
        RowPage{
            .blocks = std::move(blocks),
            .lru_it = lru_it,
            .bytes = bytes
        }
    ).first;

    cached_bytes_ += bytes;
    evict_if_needed();

    return inserted_it->second.blocks;
}

std::vector<ColumnBlockMeta>& SSTableMetadataCache::get_column_page(std::uint32_t page_id) {
    auto it = column_pages_.find(page_id);
    if (it != column_pages_.end()) {
        touch_page(CacheKey{.kind = PageKind::ColumnMetadata, .page_id = page_id}, it->second.lru_it);
        return it->second.blocks;
    }

    const auto first_block = page_first_block(page_id);
    const auto block_count = page_block_count(page_id);

    auto blocks = reader_.read_column_metadata_range(
        first_block,
        block_count,
        columns_count_
    );
    const auto bytes = column_page_bytes(blocks);

    CacheKey key{
        .kind = PageKind::ColumnMetadata,
        .page_id = page_id,
        .column_idx = 0
    };

    std::list<CacheKey>::iterator lru_it;
    insert_lru_front(key, lru_it);

    auto inserted_it = column_pages_.emplace(
        page_id,
        ColumnPage{
            .blocks = std::move(blocks),
            .lru_it = lru_it,
            .bytes = bytes
        }
    ).first;

    cached_bytes_ += bytes;
    evict_if_needed();

    return inserted_it->second.blocks;
}

SSTableMetadataCache::StatsPage& SSTableMetadataCache::get_stats_page(
    std::size_t column_idx,
    std::uint32_t page_id
) {
    const CacheKey key{
        .kind = PageKind::NumericStats,
        .page_id = page_id,
        .column_idx = column_idx
    };

    auto it = stats_pages_.find(key);
    if (it != stats_pages_.end()) {
        touch_page(key, it->second.lru_it);
        return it->second;
    }

    const auto first_block = page_first_block(page_id);
    const auto block_count = page_block_count(page_id);

    std::optional<std::vector<NumericBlockStats>> blocks;
    auto stats = reader_.read_numeric_stats_range(first_block, block_count, {column_idx});
    auto stats_it = stats.by_column.find(column_idx);
    if (stats_it != stats.by_column.end()) {
        blocks = std::move(stats_it->second);
    }

    const auto bytes = stats_page_bytes(blocks);

    std::list<CacheKey>::iterator lru_it;
    insert_lru_front(key, lru_it);

    auto inserted_it = stats_pages_.emplace(
        key,
        StatsPage{
            .blocks = std::move(blocks),
            .lru_it = lru_it,
            .bytes = bytes
        }
    ).first;

    cached_bytes_ += bytes;
    evict_if_needed();

    return inserted_it->second;
}

std::vector<RowBlockMeta> SSTableMetadataCache::read_row_metadata_range(
    std::uint32_t first_block_id,
    std::uint32_t block_count
) {
    if (block_count == 0) {
        return {};
    }

    const auto last_block_exclusive = first_block_id + block_count;
    if (last_block_exclusive > info_.num_blocks || last_block_exclusive < first_block_id) {
        throw std::runtime_error("Requested row metadata range is out of SSTable bounds");
    }

    std::vector<RowBlockMeta> result;
    result.reserve(block_count);

    const auto first_page = first_block_id / page_blocks_;
    const auto last_page = (last_block_exclusive - 1) / page_blocks_;

    for (auto page_id = first_page; page_id <= last_page; ++page_id) {
        auto& page = get_row_page(page_id);

        const auto page_first = page_first_block(page_id);
        const auto copy_from = std::max(first_block_id, page_first);
        const auto copy_to = std::min(last_block_exclusive, page_first + static_cast<std::uint32_t>(page.size()));

        for (auto block_id = copy_from; block_id < copy_to; ++block_id) {
            result.push_back(page[block_id - page_first]);
        }
    }

    return result;
}

std::vector<ColumnBlockMeta> SSTableMetadataCache::read_column_metadata_range(
    std::uint32_t first_block_id,
    std::uint32_t block_count
) {
    if (block_count == 0 || columns_count_ == 0) {
        return {};
    }

    const auto last_block_exclusive = first_block_id + block_count;
    if (last_block_exclusive > info_.num_blocks || last_block_exclusive < first_block_id) {
        throw std::runtime_error("Requested column metadata range is out of SSTable bounds");
    }

    std::vector<ColumnBlockMeta> result;
    result.reserve(static_cast<std::size_t>(block_count) * columns_count_);

    const auto first_page = first_block_id / page_blocks_;
    const auto last_page = (last_block_exclusive - 1) / page_blocks_;

    for (auto page_id = first_page; page_id <= last_page; ++page_id) {
        auto& page = get_column_page(page_id);

        const auto page_first = page_first_block(page_id);
        const auto copy_from = std::max(first_block_id, page_first);
        const auto copy_to = std::min(last_block_exclusive, page_first + page_block_count(page_id));

        const auto first_entry = static_cast<std::size_t>(copy_from - page_first) * columns_count_;
        const auto last_entry = static_cast<std::size_t>(copy_to - page_first) * columns_count_;

        for (auto entry_id = first_entry; entry_id < last_entry; ++entry_id) {
            result.push_back(page[entry_id]);
        }
    }

    return result;
}

NumericStatsRange SSTableMetadataCache::read_numeric_stats_range(
    std::uint32_t first_block_id,
    std::uint32_t block_count,
    const std::vector<std::size_t>& column_indices
) {
    NumericStatsRange result{
        .first_block_id = first_block_id,
        .block_count = block_count,
        .by_column = {}
    };

    if (block_count == 0 || column_indices.empty()) {
        return result;
    }

    const auto last_block_exclusive = first_block_id + block_count;
    if (last_block_exclusive > info_.num_blocks || last_block_exclusive < first_block_id) {
        throw std::runtime_error("Requested numeric stats range is out of SSTable bounds");
    }

    const auto first_page = first_block_id / page_blocks_;
    const auto last_page = (last_block_exclusive - 1) / page_blocks_;

    for (std::size_t column_idx : column_indices) {
        std::vector<NumericBlockStats> column_stats;
        column_stats.reserve(block_count);
        bool column_present = true;

        for (auto page_id = first_page; page_id <= last_page; ++page_id) {
            auto& page = get_stats_page(column_idx, page_id);
            if (!page.blocks.has_value()) {
                column_present = false;
                break;
            }

            const auto page_first = page_first_block(page_id);
            const auto copy_from = std::max(first_block_id, page_first);
            const auto copy_to = std::min(last_block_exclusive, page_first + static_cast<std::uint32_t>(page.blocks->size()));

            for (auto block_id = copy_from; block_id < copy_to; ++block_id) {
                column_stats.push_back((*page.blocks)[block_id - page_first]);
            }
        }

        if (column_present) {
            result.by_column.emplace(column_idx, std::move(column_stats));
        }
    }

    return result;
}

} 
