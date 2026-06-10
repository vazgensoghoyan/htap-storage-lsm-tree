#include "storage/read/sstable/sstable_metadata_cache.hpp"

#include <algorithm>
#include <stdexcept>

namespace htap::storage::read::sstable {

SSTableMetadataCache::SSTableMetadataCache(
    const lsmtree::SSTableInfo& info,
    std::uint32_t columns_count,
    std::uint32_t page_blocks,
    std::size_t max_cached_pages
)
    : info_(info),
    columns_count_(columns_count),
    page_blocks_(page_blocks),
    max_cached_pages_(max_cached_pages),
    reader_(info.path),
    sparse_index_(reader_.read_sparse_index()) {

    if (page_blocks_ == 0) {
        throw std::runtime_error("SSTableMetadataCache page_blocks must be positive");
    }

    if (max_cached_pages_ == 0) {
        throw std::runtime_error("SSTableMetadataCache max_cached_pages must be positive");
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

void SSTableMetadataCache::touch_row_page(std::uint32_t page_id, RowPage& page) {
    row_lru_.splice(row_lru_.begin(), row_lru_, page.lru_it);
    page.lru_it = row_lru_.begin();
}

void SSTableMetadataCache::touch_column_page(std::uint32_t page_id, ColumnPage& page) {
    column_lru_.splice(column_lru_.begin(), column_lru_, page.lru_it);
    page.lru_it = column_lru_.begin();
}

void SSTableMetadataCache::evict_row_if_needed() {
    while (row_pages_.size() > max_cached_pages_) {
        const auto victim = row_lru_.back();
        row_lru_.pop_back();
        row_pages_.erase(victim);
    }
}

void SSTableMetadataCache::evict_column_if_needed() {
    while (column_pages_.size() > max_cached_pages_) {
        const auto victim = column_lru_.back();
        column_lru_.pop_back();
        column_pages_.erase(victim);
    }
}

std::vector<RowBlockMeta>& SSTableMetadataCache::get_row_page(std::uint32_t page_id) {
    auto it = row_pages_.find(page_id);
    if (it != row_pages_.end()) {
        touch_row_page(page_id, it->second);
        return it->second.blocks;
    }

    const auto first_block = page_first_block(page_id);
    const auto block_count = page_block_count(page_id);

    auto blocks = reader_.read_row_metadata_range(first_block, block_count);

    row_lru_.push_front(page_id);

    auto [inserted_it, inserted] = row_pages_.emplace(
        page_id,
        RowPage{
            .blocks = std::move(blocks),
            .lru_it = row_lru_.begin()
        }
    );

    evict_row_if_needed();

    return inserted_it->second.blocks;
}

std::vector<ColumnBlockMeta>& SSTableMetadataCache::get_column_page(std::uint32_t page_id) {
    auto it = column_pages_.find(page_id);
    if (it != column_pages_.end()) {
        touch_column_page(page_id, it->second);
        return it->second.blocks;
    }

    const auto first_block = page_first_block(page_id);
    const auto block_count = page_block_count(page_id);

    auto blocks = reader_.read_column_metadata_range(
        first_block,
        block_count,
        columns_count_
    );

    column_lru_.push_front(page_id);

    auto inserted_it = column_pages_.emplace(
        page_id,
        ColumnPage{
            .blocks = std::move(blocks),
            .lru_it = column_lru_.begin()
        }
    ).first;

    evict_column_if_needed();

    return inserted_it->second.blocks;
}

std::vector<RowBlockMeta> SSTableMetadataCache::read_row_metadata_range(
    std::uint32_t first_block_id,
    std::uint32_t block_count
) {
    if (block_count == 0) {
        return {};
    }

    const auto last_block_exclusive = first_block_id + block_count;
    if (last_block_exclusive > info_.num_blocks) {
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
    if (last_block_exclusive > info_.num_blocks) {
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

} 
