#pragma once // lsmtree/storage_engine.hpp

#include <string>
#include <unordered_map>
#include <memory>

#include "storage/api/storage_engine_interface.hpp"

#include "lsmtree/lsmtree.hpp"

namespace htap::engine {

class StorageEngine : public storage::IStorageEngine {
public:
    StorageEngine(const std::string& dir_path);

    void create_table(const std::string& table_name, const storage::Schema& schema) override;

    bool table_exists(const std::string& table_name) const override;

    const storage::Schema& get_table_schema(const std::string& table_name) const override;

    void insert(const std::string& table_name, const storage::Row& values) override;

private:
    std::string dir_path_;
    std::unordered_map<std::string, std::unique_ptr<lsmtree::LSMTree>> tables_;
};

} // namespace htap::engine
