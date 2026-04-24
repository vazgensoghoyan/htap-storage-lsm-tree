#include "storage/mock/mock_storage_engine.hpp"
#include "storage/mock/mock_cursor.hpp"

#include <stdexcept>
#include <memory>

using namespace htap::storage;

void MockStorageEngine::create_table(const std::string& table_name, const Schema& schema) {
    if (tables_.count(table_name))
        throw std::runtime_error("Table already exists");

    tables_.emplace(table_name, MockTable{schema, {}});
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

    for (size_t i = 0; i < values.size(); ++i) {
        if (!table.schema.is_valid_value(i, values[i]))
            throw std::runtime_error("Invalid value for column");
    }

    Key key = std::get<int64_t>(*values[table.schema.key_column_index()]);
    table.data[key] = values;
}

std::unique_ptr<ICursor> MockStorageEngine::get(
    const std::string& table_name,
    Key key,
    const std::vector<size_t>& projection
) const {
    const auto& table = get_table(table_name);

    std::vector<Key> keys;
    if (table.data.find(key) != table.data.end()) {
        keys.push_back(key);
    }

    return std::make_unique<MockCursor>(
        std::move(keys),
        &table.data,
        projection
    );
}

std::unique_ptr<ICursor> MockStorageEngine::scan(
    const std::string& table_name,
    OptKey from,
    OptKey to,
    const std::vector<size_t>& projection
) const {
    const auto& table = get_table(table_name);

    std::vector<Key> keys;

    auto it = from ? table.data.lower_bound(*from)
                   : table.data.begin();

    auto end_it = table.data.end();

    for (; it != end_it; ++it) {
        if (to && it->first >= *to)
            break;
        keys.push_back(it->first);
    }

    return std::make_unique<MockCursor>(
        std::move(keys),
        &table.data,
        projection
    );
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
