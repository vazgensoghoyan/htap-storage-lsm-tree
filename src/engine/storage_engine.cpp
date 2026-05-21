#include "engine/storage_engine.hpp"

#include <filesystem>
#include <format>
#include <stdexcept>

#include "utils/logger.hpp"

using namespace htap::engine;
using namespace htap::storage;
using namespace htap::lsmtree;

StorageEngine::StorageEngine(const std::string& dir_path) : dir_path_(dir_path) {
    LOG_INFO("StorageEngine initialized on path '{}'", dir_path);
}

void StorageEngine::create_table(const std::string& table_name, const Schema& schema) {
    if (table_exists(table_name))
        throw std::runtime_error("Table already exists: " + table_name);

    std::string path = std::format("{}/{}", dir_path_, table_name);
    std::filesystem::create_directories(path);

    auto lsmtree = std::make_unique<LSMTree>(schema, path);

    tables_.emplace(table_name, std::move(lsmtree));

    LOG_INFO("Created table '{}'", table_name);
}

bool StorageEngine::table_exists(const std::string& table_name) const {
    return tables_.contains(table_name);
}

const Schema& StorageEngine::get_table_schema(const std::string& table_name) const {
    auto it = tables_.find(table_name);
    if (it == tables_.end())
        throw std::runtime_error("Table not found: " + table_name);

    return (it->second)->schema();
}

void StorageEngine::insert(const std::string& table_name, const Row& values) {
    auto it = tables_.find(table_name);
    if (it == tables_.end())
        throw std::runtime_error("Table not found: " + table_name);

    (it->second)->insert(values);

    LOG_DEBUG("Inserted row into table '{}'", table_name);
}
