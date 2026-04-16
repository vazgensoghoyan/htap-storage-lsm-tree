#include "storage/mock/mock_storage_engine.hpp"

#include <stdexcept>

using namespace htap::storage;

void MockStorageEngine::create_table(const std::string& table_name, const Schema& schema) {
    if (table_exists(table_name))
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
    const auto& schema = table.schema;

    if (values.size() != schema.size())
        throw std::runtime_error("Invalid number of values");

    for (size_t i = 0; i < values.size(); ++i) {
        if (!schema.is_valid_value(i, values[i]))
            throw std::runtime_error("Invalid value for column");
    }

    size_t key_idx = schema.key_column_index();

    if (!values[key_idx].has_value())
        throw std::runtime_error("Primary key cannot be NULL");

    const Value& key_val = values[key_idx].value();

    if (!std::holds_alternative<int64_t>(key_val))
        throw std::runtime_error("Primary key must be int64");

    Key key = std::get<int64_t>(key_val);

    table.rows[key] = values;
}

std::optional<Row> MockStorageEngine::get(
    const std::string& table_name,
    Key key,
    const std::vector<size_t>& projection) const
{
    const auto& table = get_table(table_name);

    auto it = table.rows.find(key);

    if (it == table.rows.end()) {
        return std::nullopt;
    }

    const Row& row = it->second;

    Row projected;
    projected.reserve(projection.size());

    for (size_t idx : projection) {
        if (idx >= row.size())
            throw std::runtime_error("Invalid projection index");

        projected.push_back(row[idx]);
    }

    return projected;
}

std::unique_ptr<ICursor> MockStorageEngine::scan(
    const std::string& table_name,
    std::optional<Key> from,
    std::optional<Key> to,
    const std::vector<size_t>& projection) const
{
    const auto& table = get_table(table_name);

    Key from_key = from.value_or(KEY_MIN);
    Key to_key   = to.value_or(KEY_MAX);

    if (from_key > to_key)
        throw std::runtime_error("Invalid scan range");

    return std::make_unique<MockCursor>(
        table.rows,
        projection,
        from_key,
        to_key
    );
}

MockStorageEngine::MockTable& MockStorageEngine::get_table(const std::string& table_name) {
    auto it = tables_.find(table_name);

    if (it == tables_.end())
        throw std::runtime_error("Table not found");

    return it->second;
}

const MockStorageEngine::MockTable& MockStorageEngine::get_table(const std::string& table_name) const {
    auto it = tables_.find(table_name);

    if (it == tables_.end())
        throw std::runtime_error("Table not found");

    return it->second;
}
