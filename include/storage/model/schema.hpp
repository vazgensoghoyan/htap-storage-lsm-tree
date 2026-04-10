#pragma once // storage/model/schema.hpp

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <stdexcept>

#include "storage/model/value.hpp"

namespace htap::storage {

struct Column {
    std::string name;
    ValueType type;
    bool is_key = false;
    bool nullable = true;
};

/**
 * Схема таблицы
 *
 * Ограничения:
 * - только один первычный ключ
 * - имена столбцов уникальны
 */
class Schema {
public:
    Schema() = default;

    void add_column(
        const std::string& name,
        ValueType type,
        bool is_key = false,
        bool nullable = true);

    const std::vector<Column>& columns() const;
    const Column& get_column(size_t index) const;

    size_t size() const;

    // индекс колонки по имени
    std::optional<size_t> get_column_index(const std::string& name) const;

    // индекс первичного ключа
    size_t key_column_index() const;

    // проверить соответствие значения по номеру колонки
    bool is_valid_value(size_t column_index, const NullableValue& value) const;

private:
    std::vector<Column> columns_;

    std::unordered_map<std::string, size_t> name_to_index_;

    std::optional<size_t> key_index_;
};

} // namespace htap::storage
