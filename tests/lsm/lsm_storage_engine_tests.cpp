#include "storage/lsm/lsm_storage_engine.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace htap::storage {
namespace {

Schema MakeSchema() {
    return Schema({
        Column{
            .name = "id",
            .type = ValueType::INT64,
            .is_key = true,
            .nullable = false,
        },
        Column{
            .name = "age",
            .type = ValueType::INT64,
            .is_key = false,
            .nullable = false,
        },
        Column{
            .name = "name",
            .type = ValueType::STRING,
            .is_key = false,
            .nullable = false,
        },
    });
}

NullableValue IntValue(std::int64_t value) {
    return NullableValue{Value{value}};
}

NullableValue StringValue(std::string value) {
    return NullableValue{Value{std::move(value)}};
}

Row MakeRow(Key key, std::int64_t age, std::string name) {
    return Row{
        IntValue(key),
        IntValue(age),
        StringValue(std::move(name)),
    };
}

std::filesystem::path MakeTempDir(const std::string& test_name) {
    auto path = std::filesystem::temp_directory_path()
        / ("htap_lsm_storage_engine_" + test_name);

    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);

    return path;
}

std::vector<Key> CollectKeys(ICursor& cursor) {
    std::vector<Key> keys;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }

    return keys;
}

std::vector<std::int64_t> CollectIntColumn(ICursor& cursor, std::size_t column_idx) {
    std::vector<std::int64_t> values;

    while (cursor.valid()) {
        const auto value = cursor.value(column_idx);

        EXPECT_TRUE(value.has_value());
        EXPECT_TRUE(std::holds_alternative<std::int64_t>(*value));

        values.push_back(std::get<std::int64_t>(*value));
        cursor.next();
    }

    return values;
}

} // namespace

TEST(LSMStorageEngineTest, CreateTableStoresSchema) {
    const auto dir = MakeTempDir("create_table_stores_schema");

    LSMStorageEngine storage(dir.string());

    storage.create_table("users", MakeSchema());

    EXPECT_TRUE(storage.table_exists("users"));
    EXPECT_FALSE(storage.table_exists("missing"));

    const auto& schema = storage.get_table_schema("users");

    ASSERT_EQ(schema.size(), 3);
    EXPECT_EQ(schema.get_column(0).name, "id");
    EXPECT_EQ(schema.get_column(1).name, "age");
    EXPECT_EQ(schema.get_column(2).name, "name");

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, ScanReturnsInsertedRows) {
    const auto dir = MakeTempDir("scan_returns_inserted_rows");

    LSMStorageEngine storage(dir.string());
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(3, 30, "c"));
    storage.insert("users", MakeRow(1, 10, "a"));
    storage.insert("users", MakeRow(2, 20, "b"));

    auto cursor = storage.scan(
        "users",
        std::nullopt,
        std::nullopt,
        {0, 1, 2},
        ScanOrder::KeyAscending
    );

    EXPECT_EQ(CollectKeys(*cursor), std::vector<Key>({1, 2, 3}));

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, ScanRespectsRange) {
    const auto dir = MakeTempDir("scan_respects_range");

    LSMStorageEngine storage(dir.string());
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(1, 10, "a"));
    storage.insert("users", MakeRow(2, 20, "b"));
    storage.insert("users", MakeRow(3, 30, "c"));
    storage.insert("users", MakeRow(4, 40, "d"));

    auto cursor = storage.scan(
        "users",
        2,
        4,
        {0, 1},
        ScanOrder::KeyAscending
    );

    EXPECT_EQ(CollectKeys(*cursor), std::vector<Key>({2, 3}));

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, GetReturnsSingleExistingRow) {
    const auto dir = MakeTempDir("get_returns_single_existing_row");

    LSMStorageEngine storage(dir.string());
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(1, 10, "a"));
    storage.insert("users", MakeRow(2, 20, "b"));
    storage.insert("users", MakeRow(3, 30, "c"));

    auto cursor = storage.get("users", 2, {0, 1});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 2);

    const auto age = cursor->value(1);

    ASSERT_TRUE(age.has_value());
    ASSERT_TRUE(std::holds_alternative<std::int64_t>(*age));
    EXPECT_EQ(std::get<std::int64_t>(*age), 20);

    cursor->next();
    EXPECT_FALSE(cursor->valid());

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, GetMissingKeyReturnsEmptyCursor) {
    const auto dir = MakeTempDir("get_missing_key_returns_empty_cursor");

    LSMStorageEngine storage(dir.string());
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(1, 10, "a"));
    storage.insert("users", MakeRow(3, 30, "c"));

    auto cursor = storage.get("users", 2, {0, 1});

    EXPECT_FALSE(cursor->valid());

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, ProjectionRestrictsAvailableColumns) {
    const auto dir = MakeTempDir("projection_restricts_available_columns");

    LSMStorageEngine storage(dir.string());
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(1, 10, "a"));

    auto cursor = storage.scan(
        "users",
        std::nullopt,
        std::nullopt,
        {0, 1},
        ScanOrder::KeyAscending
    );

    ASSERT_TRUE(cursor->valid());

    EXPECT_NO_THROW({
        const auto age = cursor->value(1);
        EXPECT_TRUE(age.has_value());
    });

    EXPECT_THROW(
        cursor->value(2),
        std::runtime_error
    );

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, ThrowsForMissingTable) {
    const auto dir = MakeTempDir("throws_for_missing_table");

    LSMStorageEngine storage(dir.string());

    EXPECT_THROW(
        storage.scan("missing", std::nullopt, std::nullopt, {0}, ScanOrder::Unordered),
        std::runtime_error
    );

    EXPECT_THROW(
        storage.get("missing", 1, {0}),
        std::runtime_error
    );

    std::filesystem::remove_all(dir);
}

} 
