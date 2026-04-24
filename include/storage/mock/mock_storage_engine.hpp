#pragma once // storage/mock/mock_storage_engine.hpp

#include <string>
#include <optional>
#include <map>
#include <vector>
#include <memory>

#include "storage/api/storage_engine_interface.hpp"

namespace htap::storage {

class MockStorageEngine final : public IStorageEngine {
public:
    void create_table(const std::string& table_name, const Schema& schema) override;

    bool table_exists(const std::string& table_name) const override;

    const Schema& get_table_schema(const std::string& table_name) const override;

    void insert(const std::string& table_name, const Row& values) override;

    std::unique_ptr<ICursor> get(
        const std::string& table_name,
        Key key,
        const std::vector<size_t>& projection
    ) const override;

    std::unique_ptr<ICursor> scan(
        const std::string& table_name,
        std::optional<Key> from,
        std::optional<Key> to,
        const std::vector<size_t>& projection
    ) const override;

private:
    struct MockTable {
        Schema schema;
        std::map<Key, Row> data;
    };

    MockTable& get_table(const std::string& name);
    const MockTable& get_table(const std::string& name) const;

private:
    std::unordered_map<std::string, MockTable> tables_;
};

} // namespace htap::storage
