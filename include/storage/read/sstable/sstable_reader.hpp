#pragma once

#include "storage/read/sstable/row_block_meta.hpp"
#include "storage/read/sstable/column_block_meta.hpp"
#include "lsmtree/sstable/format/sparse_index_entry.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace htap::storage::read::sstable {

class SSTableReader final {
public:
    explicit SSTableReader(std::filesystem::path path);

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

private:
    std::vector<char> read_data_bytes(std::uint64_t offset, std::uint64_t size);

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

private:
    std::filesystem::path path_;
    std::ifstream input_;
};

}