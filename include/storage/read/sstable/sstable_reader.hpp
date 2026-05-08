#pragma once

#include "storage/read/sstable/row_block_meta.hpp"
#include "storage/read/sstable/column_block_meta.hpp"

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

private:
    std::vector<char> read_bytes(std::uint64_t offset, std::uint64_t size);

private:
    std::filesystem::path path_;
    std::ifstream input_;
};

}