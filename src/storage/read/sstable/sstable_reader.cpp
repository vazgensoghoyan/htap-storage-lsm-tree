#include "storage/read/sstable/sstable_reader.hpp"

#include <cstring>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace htap::storage::read::sstable {

namespace {

constexpr std::uint32_t STATS_MAGIC = 0x53544154; // "STAT"
constexpr std::uint32_t STATS_VERSION = 1;
constexpr std::uint32_t STATS_HEADER_SIZE = 16;
constexpr std::uint32_t STATS_COLUMN_DESCRIPTOR_SIZE = 17;
constexpr std::uint32_t NUMERIC_STATS_ENTRY_SIZE = 17;

struct StatsColumnDescriptor {
    std::uint32_t column_idx;
    ValueType type;
    std::uint64_t offset;
    std::uint32_t entry_size;
};

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
    : paths_(std::move(path)) {
    input_.open(data_path(), std::ios::binary);
    if (!input_.is_open()) {
        throw std::runtime_error("Cannot open SSTable data file: " + data_path().string());
    }
}

const std::filesystem::path& SSTableReader::path() const noexcept {
    return paths_.dir();
}

std::filesystem::path SSTableReader::data_path() const {
    return paths_.data();
}

std::filesystem::path SSTableReader::index_path() const {
    return paths_.index();
}

std::filesystem::path SSTableReader::metadata_path() const {
    return paths_.meta();
}

std::filesystem::path SSTableReader::stats_path() const {
    return paths_.stats();
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

NumericStatsRange SSTableReader::read_numeric_stats_range(
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

    const auto path = stats_path();
    if (!std::filesystem::exists(path)) {
        return result;
    }

    const auto file_size = std::filesystem::file_size(path);
    if (file_size < STATS_HEADER_SIZE) {
        throw std::runtime_error("Invalid stats file size: " + path.string());
    }

    const auto header = read_bytes_from_file(path, 0, STATS_HEADER_SIZE);
    std::size_t header_pos = 0;

    const auto magic = read_value<std::uint32_t>(header, header_pos);
    const auto version = read_value<std::uint32_t>(header, header_pos);
    const auto num_blocks = read_value<std::uint32_t>(header, header_pos);
    const auto num_stats_columns = read_value<std::uint32_t>(header, header_pos);

    if (magic != STATS_MAGIC) {
        throw std::runtime_error("Invalid stats file magic: " + path.string());
    }

    if (version != STATS_VERSION) {
        throw std::runtime_error("Unsupported stats file version: " + path.string());
    }

    if (first_block_id > num_blocks || block_count > num_blocks - first_block_id) {
        throw std::runtime_error("Stats block range is out of file bounds: " + path.string());
    }

    const auto descriptors_size =
        static_cast<std::uint64_t>(num_stats_columns) * STATS_COLUMN_DESCRIPTOR_SIZE;

    if (file_size < STATS_HEADER_SIZE + descriptors_size) {
        throw std::runtime_error("Invalid stats descriptors size: " + path.string());
    }

    const auto descriptors_data = read_bytes_from_file(path, STATS_HEADER_SIZE, descriptors_size);
    std::size_t descriptor_pos = 0;
    std::vector<StatsColumnDescriptor> descriptors;
    descriptors.reserve(num_stats_columns);

    for (std::uint32_t i = 0; i < num_stats_columns; ++i) {
        const auto column_idx = read_value<std::uint32_t>(descriptors_data, descriptor_pos);
        const auto raw_type = read_value<std::uint8_t>(descriptors_data, descriptor_pos);
        const auto offset = read_value<std::uint64_t>(descriptors_data, descriptor_pos);
        const auto entry_size = read_value<std::uint32_t>(descriptors_data, descriptor_pos);

        ValueType type;
        switch (static_cast<ValueType>(raw_type)) {
            case ValueType::INT64:
                type = ValueType::INT64;
                break;
            case ValueType::DOUBLE:
                type = ValueType::DOUBLE;
                break;
            default:
                throw std::runtime_error("Invalid stats value type: " + path.string());
        }

        if (entry_size != NUMERIC_STATS_ENTRY_SIZE) {
            throw std::runtime_error("Invalid stats entry size: " + path.string());
        }

        const auto column_data_size =
            static_cast<std::uint64_t>(num_blocks) * entry_size;

        if (offset > file_size || column_data_size > file_size - offset) {
            throw std::runtime_error("Stats column data is out of file bounds: " + path.string());
        }

        descriptors.push_back(StatsColumnDescriptor{
            .column_idx = column_idx,
            .type = type,
            .offset = offset,
            .entry_size = entry_size
        });
    }

    for (std::size_t requested_column : column_indices) {
        const auto descriptor_it = std::find_if(
            descriptors.begin(),
            descriptors.end(),
            [requested_column](const auto& descriptor) {
                return descriptor.column_idx == requested_column;
            }
        );

        if (descriptor_it == descriptors.end()) {
            continue;
        }

        const auto offset =
            descriptor_it->offset +
            static_cast<std::uint64_t>(first_block_id) * descriptor_it->entry_size;
        const auto size =
            static_cast<std::uint64_t>(block_count) * descriptor_it->entry_size;
        const auto data = read_bytes_from_file(path, offset, size);

        std::size_t pos = 0;
        std::vector<NumericBlockStats> column_stats;
        column_stats.reserve(block_count);

        for (std::uint32_t i = 0; i < block_count; ++i) {
            const auto has_value = read_value<std::uint8_t>(data, pos) != 0;

            NumericBlockStats stats{
                .column_idx = descriptor_it->column_idx,
                .type = descriptor_it->type,
                .has_value = has_value,
                .min_value = std::int64_t{0},
                .max_value = std::int64_t{0}
            };

            if (descriptor_it->type == ValueType::INT64) {
                stats.min_value = read_value<std::int64_t>(data, pos);
                stats.max_value = read_value<std::int64_t>(data, pos);
            } else {
                stats.min_value = read_value<double>(data, pos);
                stats.max_value = read_value<double>(data, pos);
            }

            column_stats.push_back(std::move(stats));
        }

        result.by_column.emplace(requested_column, std::move(column_stats));
    }

    return result;
}

} 
