#pragma once // storage/mock/mock_storage_engine.hpp

#include <memory>
#include <string>
#include <unordered_map>
#include <map>
#include <vector>

#include "storage/api/storage_engine_interface.hpp"
#include "storage/model/schema.hpp"
#include "storage/model/value.hpp"
#include "storage/mock/mock_cursor.hpp"

namespace htap::storage {

class MockStorageEngine : public IStorageEngine {
public:
    MockStorageEngine() = default;
    ~MockStorageEngine() override = default;

    void create_table(const std::string& table_name, const Schema& schema) override;

    bool table_exists(const std::string& table_name) const override;

    const Schema& get_table_schema(const std::string& table_name) const override;

    void insert(
        const std::string& table_name,
        const std::vector<NullableValue>& values) override;

    std::optional<Row> get(
        const std::string& table_name,
        Key key,
        const std::vector<size_t>& projection) const override;

    std::unique_ptr<ICursor> scan(
        const std::string& table_name,
        std::optional<Key> from,
        std::optional<Key> to,
        const std::vector<size_t>& projection) const override;

private:
    struct Table {
        Schema schema;
        std::map<Key, Row> rows;
    };

private:
    // Получить таблицу (ошибка если не существует)
    Table& get_table(const std::string& table_name);

    const Table& get_table(const std::string& table_name) const;

private:
    std::unordered_map<std::string, Table> tables_;
};

} // namespace htap::storage
