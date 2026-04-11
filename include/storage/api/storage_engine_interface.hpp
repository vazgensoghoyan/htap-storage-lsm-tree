#pragma once // storage/api/storage_engine_interface.hpp

#include <memory>
#include <optional>
#include <vector>

#include "storage/model/schema.hpp"
#include "storage/model/value.hpp"
#include "storage/api/cursor_interface.hpp"

namespace htap::storage {

/**
 * StorageEngine представляет собой хранилище на одну таблицу
 *
 * Каждый объект имеет свою схему, которую он получает при создании
 * 
 * В реализациях храним примерно
 * std::unordered_map<std::string, std::unique_ptr<LsmTree>> tables;
 * 
 */
class IStorageEngine {
public:
    virtual ~IStorageEngine() = default;

    // ошибка если таблица уже существует
    virtual void create_table(const std::string& table_name, const Schema& schema) = 0;

    virtual bool table_exists(const std::string& table_name) const = 0;

    // в нижних методах везде ошибка если таблицы не существует

    virtual const Schema& get_table_schema(const std::string& table_name) const = 0;

    // данные должны соответствовать схеме, ошибка в insert иначе
    // projection:
    // - содержит уникальные индексы колонок < schema.size()
    // - порядок сохраняется (как в запросе, типо select b, a)

    virtual void insert(
        const std::string& table_name,
        Key key,
        const std::vector<NullableValue>& values) = 0;

    virtual std::unique_ptr<ICursor> get(
        const std::string& table_name,
        Key key,
        const std::vector<size_t>& projection) const = 0;

    // [from, to)
    // решил, что так лучше, как например для складывания диапозонов
    virtual std::unique_ptr<ICursor> scan(
        const std::string& table_name,
        Key from,
        Key to,
        const std::vector<size_t>& projection) const = 0;
};

} // namespace htap::storage
