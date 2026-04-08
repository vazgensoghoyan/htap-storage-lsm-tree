#pragma once // storage/api/storage_engine.hpp

#include <memory>
#include <optional>
#include <vector>

#include "storage/model/schema.hpp"
#include "storage/model/value.hpp"
#include "storage/api/cursor.hpp"

namespace htap::storage {

/**
 * StorageEngine представляет собой хранилище на одну таблицу
 *
 * Каждый объект имеет свою схему, которую он получает при создании
 * 
 * А уже executor, я предлагаю, будет хранить например:
 * std::unordered_map<std::string, std::unique_ptr<StorageEngine>> tables;
 */
class IStorageEngine {
public:
    explicit IStorageEngine(const Schema& schema);
    virtual ~IStorageEngine() = default;

    const Schema& schema() const;

    virtual void insert(
        int64_t key,
        const std::vector<std::optional<Value>>& values) = 0;

    virtual std::unique_ptr<Cursor> get(
        int64_t key,
        const std::vector<size_t>& projection) const = 0;

    virtual std::unique_ptr<Cursor> scan(
        int64_t from,
        int64_t to,
        const std::vector<size_t>& projection) const = 0;

protected:
    Schema schema_;
};

} // namespace htap::storage
