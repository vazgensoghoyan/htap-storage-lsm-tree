#pragma once // lsmtree/sstable/sstable_paths.hpp

#include <filesystem>
#include <utility>

namespace htap::lsmtree::sstable {

class SSTablePaths {
public:
    explicit SSTablePaths(std::filesystem::path dir) : dir_(std::move(dir)) {}

    const std::filesystem::path& dir() const { return dir_; }

    std::filesystem::path data() const { return dir_ / "data.sst"; }

    std::filesystem::path meta() const { return dir_ / "meta.bin"; }

    std::filesystem::path index() const { return dir_ / "sparse.idx"; }

    std::filesystem::path info() const { return dir_ / "info.bin"; }

private:
    std::filesystem::path dir_;
};

} // namespace htap::lsmtree::sstable
