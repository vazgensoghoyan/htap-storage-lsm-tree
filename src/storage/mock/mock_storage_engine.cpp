#include "storage/mock/mock_storage_engine.hpp"
#include "storage/mock/mock_cursor.hpp"
#include "utils/logger.hpp"

#include <stdexcept>
#include <memory>

using namespace htap::storage;

void MockStorageEngine::create_table(const std::string& table_name, const Schema& schema) {
    if (tables_.count(table_name))
        throw std::runtime_error("Table already exists");

    tables_.emplace(table_name, MockTable{schema, {}});

    LOG_INFO("create_table: name='{}', columns={}", table_name, schema.size());
}

bool MockStorageEngine::table_exists(const std::string& table_name) const {
    return tables_.contains(table_name);
}

const Schema& MockStorageEngine::get_table_schema(const std::string& table_name) const {
    return get_table(table_name).schema;
}

void MockStorageEngine::insert(const std::string& table_name, const Row& values) {
    auto& table = get_table(table_name);

    if (values.size() != table.schema.size())
        throw std::runtime_error("Row size mismatch schema");

    for (size_t i = 0; i < values.size(); ++i)
        if (!table.schema.is_valid_value(i, values[i]))
            throw std::runtime_error("Invalid value for column");

    const auto& cell = values[KEY_COLUMN_INDEX];

    if (!cell.has_value())
        throw std::runtime_error("Key cannot be NULL");

    if (!std::holds_alternative<int64_t>(*cell))
        throw std::runtime_error("Key must be int64");

    Key key = std::get<int64_t>(*cell);

    #ifdef LOGGING_ENABLED
    if (table.data.contains(key))
        LOG_WARN("insert overwrite: table='{}', key={}", table_name, key);
    #endif

    table.data[key] = values;

    LOG_DEBUG("insert: table='{}', key={}, size after={}", table_name, key, table.data.size());
}

std::unique_ptr<ICursor> MockStorageEngine::get(const std::string& table_name, Key key, const std::vector<size_t>&) const {
    const auto& table = get_table(table_name);
    LOG_DEBUG("get: table='{}', key={}", table_name, key);
    return std::make_unique<MockCursor>(&table.data, key, key + 1);
}

std::unique_ptr<ICursor> MockStorageEngine::scan(const std::string& table_name, OptKey from, OptKey to, const std::vector<size_t>&) const {
    const auto& table = get_table(table_name);
    LOG_DEBUG("scan: table='{}', from={}, to={}",
        table_name,
        from.has_value() ? std::to_string(*from) : "-inf",
        to.has_value() ? std::to_string(*to) : "+inf"
    );
    return std::make_unique<MockCursor>(&table.data, from, to);
}

MockStorageEngine::MockTable& MockStorageEngine::get_table(const std::string& name) {
    auto it = tables_.find(name);

    if (it == tables_.end())
        throw std::runtime_error("Table does not exist");

    return it->second;
}

const MockStorageEngine::MockTable& MockStorageEngine::get_table(const std::string& name) const {
    auto it = tables_.find(name);

    if (it == tables_.end())
        throw std::runtime_error("Table does not exist");

    return it->second;
}
