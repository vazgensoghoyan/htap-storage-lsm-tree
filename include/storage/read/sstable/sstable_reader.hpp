#pragma once

#include "storage/read/sstable/row_block_meta.hpp"
#include "storage/read/sstable/column_block_meta.hpp"
#include "storage/read/sstable/numeric_stats.hpp"
#include "storage/read/sstable/sstable_block_cache.hpp"
#include "lsmtree/sstable/format/sparse_index_entry.hpp"
#include "lsmtree/sstable/sstable_paths.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace htap::storage::read::sstable {

class SSTableReader final {
public:
    explicit SSTableReader(
        std::filesystem::path path,
        std::shared_ptr<SSTableBlockCache> block_cache = nullptr,
        std::uint64_t sstable_id = 0
    );

    const std::filesystem::path& path() const noexcept;

    std::vector<char> read_block(const RowBlockMeta& block);
    std::vector<char> read_block(const ColumnBlockMeta& block);

    std::vector<lsmtree::sstable::SparseIndexEntry> read_sparse_index();

    std::vector<RowBlockMeta> read_row_metadata_range(
        std::uint32_t first_block_id,
        std::uint32_t block_count
    );

    std::vector<ColumnBlockMeta> read_column_metadata_range(
        std::uint32_t first_block_id,
        std::uint32_t block_count,
        std::uint32_t columns_count
    );

    NumericStatsRange read_numeric_stats_range(
        std::uint32_t first_block_id,
        std::uint32_t block_count,
        const std::vector<std::size_t>& column_indices
    );

private:
    struct NumericStatsColumnDescriptor {
        std::uint32_t column_idx;
        ValueType type;
        std::uint64_t offset;
        std::uint32_t entry_size;
    };

    std::vector<char> read_data_bytes(std::uint64_t offset, std::uint64_t size);
    void load_numeric_stats_index();

    static std::vector<char> read_bytes_from_file(
        const std::filesystem::path& path,
        std::uint64_t offset,
        std::uint64_t size
    );

    static std::vector<char> read_bytes_from_stream(
        std::istream& input,
        const std::filesystem::path& path,
        std::uint64_t offset,
        std::uint64_t size
    );

    std::filesystem::path data_path() const;
    std::filesystem::path index_path() const;
    std::filesystem::path metadata_path() const;
    std::filesystem::path stats_path() const;

private:
    lsmtree::sstable::SSTablePaths paths_;
    std::ifstream input_;
    std::shared_ptr<SSTableBlockCache> block_cache_;
    std::uint64_t sstable_id_ = 0;

    bool numeric_stats_index_loaded_ = false;
    bool numeric_stats_file_exists_ = false;
    std::uint32_t numeric_stats_num_blocks_ = 0;
    std::vector<NumericStatsColumnDescriptor> numeric_stats_descriptors_;
};

}
