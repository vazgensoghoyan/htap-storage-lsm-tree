#pragma once // storage/model/schema_builder.hpp

#include <string>
#include <vector>
#include <unordered_set>

#include "storage/model/schema.hpp"

namespace htap::storage {

/**
 * Builder для создания Schema
 *
 * Использование:
 *
 * Schema schema = SchemaBuilder()
 *     .add_column("id", ValueType::INT64, true, false)
 *     .add_column("name", ValueType::STRING, false, true)
 *     .build();
 *
 * После build() Schema становится immutable
 */
class SchemaBuilder {
public:
    SchemaBuilder();

    // Добавить колонку
    // Исключение, если с таким именем уже существует
    SchemaBuilder& add_column(
        std::string name,
        ValueType type,
        bool is_key = false,
        bool nullable = true);

    // Возможно TODO: key только 1 колонка

    // Построить Schema
    //
    // Выполняет валидацию:
    // - есть хотя бы одна колонка
    // - ровно один primary key
    // - key обязательно типа int64 (пока мы решили так действовать) и не nullable
    // - key обазятельно на первом месте
    //
    // исключение при нарушении этих условий
    Schema build();

private:
    std::vector<Column> columns_;
    std::unordered_set<std::string> column_names_;
    bool key_seen_;
};

} // namespace htap::storage
