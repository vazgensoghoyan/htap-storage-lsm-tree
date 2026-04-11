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
 * Возвращает все подходящие данные в порядке возрастания ключа
 *
 * Курсор принадлежит executor-у через unique_ptr
 */
class ICursor {
public:
    virtual ~ICursor() = default;

    // Указываем ли на валидную строку
    // остальные можно вызывать только если valid() == true
    virtual bool valid() const = 0;

    // если достигли конца, valid станет возвращать false
    virtual void next() = 0;

    // перейти к первому >= key
    // если такой нет, то valid() == false
    virtual void seek(const Key& key) = 0;

    // текущий ключ
    virtual Key key() const = 0;

    // проверить на null колонку
    // можно только если column_idx принадлежит projection, иначе ошибка
    virtual bool is_null(size_t column_idx) const = 0;

    // получить значение колонки
    // если значение NULL то ошибка
    // можно только если column_idx принадлежит projection, иначе ошибка
    // column_idx на индекс изначальной схемы, а не проекции
    virtual const Value& value(size_t column_idx) const = 0;

    // информация о projection
    // virtual const std::vector<size_t>& projection() const = 0;
    // пока не уверен  нужен ли этот геттер в интерфейсе
    // пока закомментировал, чтобы видно было, что уж во внутреннем
    // представлении projection быть должен для оптимизаций, кажется, это логично
};

} // namespace htap::storage
