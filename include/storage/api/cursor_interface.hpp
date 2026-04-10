#pragma once // storage/api/cursor_interface.hpp

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "storage/model/value.hpp"

namespace htap::storage {

/**
 * Курсор это как бы forward-only итератор
 *
 * нужен, чтобы:
 * - избежать копирования строк
 * - поддерживать проекцию
 * - эффективно работать с LSM-хранилищем
 *
 * Курсор принадлежит executor-у через unique_ptr
 */
class ICursor {
public:
    virtual ~ICursor() = default;

    // Указываем ли на валидную строку
    virtual bool valid() const = 0;

    // если достигли конца, valid станет возвращать false
    virtual void next() = 0;

    // перейти к первому >= key
    virtual void seek(const Key& key) = 0;

    // текущий ключ
    virtual Key key() const = 0;

    // проверить на null колонку
    virtual bool is_null(size_t column_idx) const = 0;

    // получить значение колонки
    // если значение NULL то ошибка
    // если column_idx невалиден, то ошибка
    virtual const Value& value(size_t column_idx) const = 0;

    // информация о projection
    virtual const std::vector<size_t>& projection() const = 0;
};

} // namespace htap::storage
