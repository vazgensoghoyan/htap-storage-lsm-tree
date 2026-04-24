#pragma once // storage/model/schema.hpp

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <stdexcept>

#include "storage/api/types.hpp"

namespace htap::storage {

// Описание одной колонки таблицы
struct Column {
    std::string name;
    ValueType type;
    bool is_key = false;
    bool nullable = true;
};

/**
 * Immutable схема таблицы
 *
 * Ограничения:
 * - только один первычный ключ
 * - имена столбцов уникальны
 *
 * Можно создавать напрямую или через builder
 */
class Schema {
public:
    Schema(std::vector<Column> columns);

    // Все колонки в порядке объявления
    const std::vector<Column>& columns() const noexcept;

    // Получить колонку по индексу
    // Ошибка при плохом индексе
    const Column& get_column(size_t index) const;

    // Количество колонок
    size_t size() const noexcept;

    // Проверка значения на соответствие схеме
    bool is_valid_value(size_t column_index, const NullableValue& value) const;

private:
    std::vector<Column> columns_;
};

} // namespace htap::storage
