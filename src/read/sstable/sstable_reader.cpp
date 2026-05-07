#include "storage/read/sstable/sstable_reader.hpp"

#include <stdexcept>
#include <utility>

namespace htap::storage::read::sstable {

SSTableReader::SSTableReader(std::filesystem::path path)
    : path_(std::move(path)) {
    input_.open(path_, std::ios::binary);

    if (!input_.is_open()) {
        throw std::runtime_error("Cannot open SSTable file: " + path_.string());
    }
}

const std::filesystem::path& SSTableReader::path() const noexcept {
    return path_;
}

std::vector<char> SSTableReader::read_block(const BlockMeta& block) {
    if (!input_.is_open()) {
        throw std::runtime_error("SSTable file is not open: " + path_.string());
    }

    std::vector<char> buffer(static_cast<std::size_t>(block.size));

    if (buffer.empty()) {
        return buffer;
    }

    input_.clear();
    input_.seekg(static_cast<std::streamoff>(block.offset), std::ios::beg);

    if (!input_) {
        throw std::runtime_error("Cannot seek SSTable file: " + path_.string());
    }

    input_.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    if (!input_ || input_.gcount() != static_cast<std::streamsize>(buffer.size())) {
        throw std::runtime_error("Cannot read SSTable block: " + path_.string());
    }

    return buffer;
}

} 
