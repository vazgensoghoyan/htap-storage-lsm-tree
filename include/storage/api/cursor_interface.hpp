#pragma once // storage/api/cursor_interface.hpp

#include <cstddef>
#include <cstdint>
#include <vector>

#include "storage/api/types.hpp"

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

    // текущий ключ
    virtual Key key() const = 0;

    /**
     * Получить значение колонки
     *
     * - column_idx — индекс в исходной схеме (НЕ в projection)
     * - колонка должна входить в projection, иначе ошибка
     *
     * Возвращает NullableValue:
     * - !has_value() => NULL
     * - has_value() => значение внутри variant
     */
    virtual NullableValue value(size_t column_idx) const = 0;

    // метод возвращающий весь row текущий
    // лучше не использовать когда нет нужды или когда
    // не уверен, что ты находишься в месте построчного хранения
    virtual const Row& row() const = 0; 
};

} // namespace htap::storage
