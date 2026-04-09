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
 * А уже executor, я предлагаю, будет хранить например:
 * std::unordered_map<std::string, std::unique_ptr<StorageEngine>> tables;
 */
class IStorageEngine {
public:
    explicit IStorageEngine(const Schema& schema) : schema_(std::move(schema)) {}

    virtual ~IStorageEngine() = default;

    const Schema& schema() const { return schema_; }

    virtual void insert(
        int64_t key,
        const std::vector<NullableValue>& values) = 0;

    virtual std::unique_ptr<ICursor> get(
        int64_t key,
        const std::vector<size_t>& projection) const = 0;

    virtual std::unique_ptr<ICursor> scan(
        int64_t from,
        int64_t to,
        const std::vector<size_t>& projection) const = 0;

protected:
    Schema schema_;
};

} // namespace htap::storage
