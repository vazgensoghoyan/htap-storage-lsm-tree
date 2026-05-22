#include "storage/read/sstable/sstable_reader.hpp"

#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace htap::storage::read::sstable {

namespace {

template <typename T>
T read_value(const std::vector<char>& data, std::size_t& pos) {
    if (pos + sizeof(T) > data.size()) {
        throw std::runtime_error("Unexpected end of SSTable metadata");
    }

    T value;
    std::memcpy(&value, data.data() + pos, sizeof(T));
    pos += sizeof(T);

    return value;
}

}

SSTableReader::SSTableReader(std::filesystem::path path)
    : path_(std::move(path)) {
    input_.open(data_path(), std::ios::binary);

    if (!input_.is_open()) {
        throw std::runtime_error("Cannot open SSTable file: " + data_path().string());
    }
}

const std::filesystem::path& SSTableReader::path() const noexcept {
    return path_;
}

std::filesystem::path SSTableReader::data_path() const {
    auto path = path_;
    path.replace_extension(".sst");
    return path;
}

std::filesystem::path SSTableReader::index_path() const {
    auto path = path_;
    path.replace_extension(".idx");
    return path;
}

std::filesystem::path SSTableReader::metadata_path() const {
    auto path = path_;
    path.replace_extension(".meta");
    return path;
}

std::vector<char> SSTableReader::read_block(const RowBlockMeta& block) {
    return read_data_bytes(block.offset, block.size);
}

std::vector<char> SSTableReader::read_block(const ColumnBlockMeta& block) {
    return read_data_bytes(block.offset, block.size);
}

std::vector<char> SSTableReader::read_bytes_from_stream(
    std::istream& input,
    const std::filesystem::path& path,
    std::uint64_t offset,
    std::uint64_t size
) {
    std::vector<char> buffer(static_cast<std::size_t>(size));

    if (buffer.empty()) {
        return buffer;
    }

    input.clear();
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

    if (!input) {
        throw std::runtime_error("Cannot seek file: " + path.string());
    }

    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    if (!input || input.gcount() != static_cast<std::streamsize>(buffer.size())) {
        throw std::runtime_error("Cannot read file: " + path.string());
    }

    return buffer;
}

std::vector<char> SSTableReader::read_data_bytes(std::uint64_t offset, std::uint64_t size) {
    if (!input_.is_open()) {
        throw std::runtime_error("SSTable file is not open: " + data_path().string());
    }

    return read_bytes_from_stream(input_, data_path(), offset, size);
}

std::vector<char> SSTableReader::read_bytes_from_file(
    const std::filesystem::path& path,
    std::uint64_t offset,
    std::uint64_t size
) {
    std::ifstream input(path, std::ios::binary);

    if (!input.is_open()) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }

    return read_bytes_from_stream(input, path, offset, size);
}

std::vector<lsmtree::sstable::SparseIndexEntry> SSTableReader::read_sparse_index() {
    const auto path = index_path();

    if (!std::filesystem::exists(path)) {
        return {};
    }

    const auto index_size = std::filesystem::file_size(path);

    if (index_size == 0) {
        return {};
    }

    if (index_size % lsmtree::sstable::SPARSE_INDEX_ENTRY_ON_DISK_SIZE != 0) {
        throw std::runtime_error("Invalid sparse index file size: " + path.string());
    }

    const auto data = read_bytes_from_file(path, 0, index_size);
    std::size_t pos = 0;
    std::size_t entry_count = index_size / lsmtree::sstable::SPARSE_INDEX_ENTRY_ON_DISK_SIZE;
    std::vector<lsmtree::sstable::SparseIndexEntry> index;
    index.reserve(entry_count);

    for (std::uint64_t i = 0; i < entry_count; ++i) {
        auto min_key = read_value<Key>(data, pos);
        auto block_id = read_value<std::uint32_t>(data, pos);
        index.push_back({min_key, block_id});
    }

    return index;
}

std::vector<RowBlockMeta> SSTableReader::read_row_metadata_range(
    std::uint32_t first_block_id,
    std::uint32_t block_count
) {
    if (block_count == 0) {
        return {};
    }

    const auto offset =
        static_cast<std::uint64_t>(first_block_id) * ROW_BLOCK_META_ON_DISK_SIZE;

    const auto size =
        static_cast<std::uint64_t>(block_count) * ROW_BLOCK_META_ON_DISK_SIZE;

    const auto data = read_bytes_from_file(metadata_path(), offset, size);

    std::size_t pos = 0;
    std::vector<RowBlockMeta> blocks;
    blocks.reserve(block_count);

    for (std::uint32_t i = 0; i < block_count; ++i) {
        auto min_key = read_value<Key>(data, pos);
        auto max_key = read_value<Key>(data, pos);
        auto block_offset = read_value<std::uint64_t>(data, pos);
        auto size_bytes = read_value<std::uint64_t>(data, pos);
        auto row_count = read_value<std::uint32_t>(data, pos);
        auto block_id = read_value<std::uint32_t>(data, pos);

        blocks.push_back(RowBlockMeta{
            .min_key = min_key,
            .max_key = max_key,
            .offset = block_offset,
            .size = size_bytes,
            .row_count = row_count,
            .block_id = static_cast<std::size_t>(block_id)
        });
    }

    return blocks;
}

std::vector<ColumnBlockMeta> SSTableReader::read_column_metadata_range(
    std::uint32_t first_block_id,
    std::uint32_t block_count,
    std::uint32_t columns_count
) {
    if (block_count == 0 || columns_count == 0) {
        return {};
    }

    const auto first_entry_id = static_cast<std::uint64_t>(first_block_id) * columns_count;

    const auto entry_count = static_cast<std::uint64_t>(block_count) * columns_count;

    const auto offset = first_entry_id * COLUMN_BLOCK_META_ON_DISK_SIZE;

    const auto size = entry_count * COLUMN_BLOCK_META_ON_DISK_SIZE;

    const auto data = read_bytes_from_file(metadata_path(), offset, size);

    std::size_t pos = 0;
    std::vector<ColumnBlockMeta> blocks;
    blocks.reserve(static_cast<std::size_t>(entry_count));

    for (std::uint64_t i = 0; i < entry_count; ++i) {
        auto min_key = read_value<Key>(data, pos);
        auto max_key = read_value<Key>(data, pos);
        auto block_offset = read_value<std::uint64_t>(data, pos);
        auto size_bytes = read_value<std::uint64_t>(data, pos);
        auto values_count = read_value<std::uint32_t>(data, pos);
        auto block_id = read_value<std::uint32_t>(data, pos);
        auto column_idx = read_value<std::uint32_t>(data, pos);

        blocks.push_back(ColumnBlockMeta{
            .min_key = min_key,
            .max_key = max_key,
            .offset = block_offset,
            .size = size_bytes,
            .values_count = values_count,
            .block_id = static_cast<std::size_t>(block_id),
            .column_idx = static_cast<std::size_t>(column_idx)
        });
    }

    return blocks;
}

} 
