#pragma once // storage/model/schema_builder.hpp

#include <string>
#include <vector>
#include <unordered_set>

#include "storage/model/schema.hpp"

namespace htap::storage {

/**
 * Builder для создания Schema
 *
 * ВАЖНО:
 * - key НЕ задаётся в schema
 * - key живёт в LSM engine (MemTable / SSTable)
 *
 * Использование:
 *
 * Schema schema = SchemaBuilder()
 *     .add_column("age", ValueType::INT64, true)
 *     .add_column("name", ValueType::STRING, false)
 *     .build();
 *
 * После build() Schema становится immutable
 */
class SchemaBuilder {
public:
    SchemaBuilder() = default;

    // Добавить колонку payload
    // Исключение, если с таким именем уже существует
    SchemaBuilder& add_column(
        std::string name,
        ValueType type,
        bool nullable);

    // Построить Schema
    // Ошибка если 0 колонок
    Schema build();

private:
    std::vector<Column> columns_;
    std::unordered_set<std::string> column_names_;
};

} // namespace htap::storage
