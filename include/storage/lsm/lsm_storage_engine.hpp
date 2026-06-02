#pragma once

#include "storage/api/storage_engine_interface.hpp"
#include "lsmtree/lsmtree.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace htap::storage {

class LSMStorageEngine final : public IStorageEngine {
public:
    LSMStorageEngine(
        std::string root_path, 
        std::size_t memtable_threshold = htap::lsmtree::DEFAULT_MEMTABLE_THRESHOLD
    );

    void create_table(const std::string& table_name, const Schema& schema) override;

    bool table_exists(const std::string& table_name) const override;

    const Schema& get_table_schema(const std::string& table_name) const override;

    void insert(const std::string& table_name, const Row& values) override;

    virtual std::unique_ptr<ICursor> get(
        const std::string& table_name,
        Key key,
        const std::vector<size_t>& projection) const override;

    virtual std::unique_ptr<ICursor> scan(
        const std::string& table_name,
        std::optional<Key> from,
        std::optional<Key> to,
        const std::vector<size_t>& projection,
        ScanOrder order = ScanOrder::Unordered) const override;

private:
    htap::lsmtree::LSMTree& get_tree(const std::string& table_name);
    const htap::lsmtree::LSMTree& get_tree(const std::string& table_name) const;

    std::string table_path(const std::string& table_name) const;

private:
    std::string root_path_;
    std::size_t memtable_threshold_;
    std::unordered_map<std::string, std::unique_ptr<htap::lsmtree::LSMTree>> tables_;
};

}