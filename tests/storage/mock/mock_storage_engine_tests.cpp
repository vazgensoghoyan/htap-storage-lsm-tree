#include <gtest/gtest.h>

#include "storage/mock/mock_storage_engine.hpp"
#include "storage/model/schema_builder.hpp"

using namespace htap::storage;

// -------------------- helper --------------------

static Schema make_schema() {
    return SchemaBuilder()
        .add_column("name", ValueType::STRING, true)
        .add_column("age", ValueType::INT64, true)
        .build();
}

// -------------------- ENGINE TESTS --------------------

TEST(MockEngine, CreateTableAndSchema) {
    MockStorageEngine engine;

    auto schema = make_schema();
    engine.create_table("users", schema);

    EXPECT_TRUE(engine.table_exists("users"));
    EXPECT_EQ(engine.get_table_schema("users").size(), 2);
}

TEST(MockEngine, DuplicateTableThrows) {
    MockStorageEngine engine;

    auto schema = make_schema();
    engine.create_table("users", schema);

    EXPECT_THROW(engine.create_table("users", schema), std::runtime_error);
}

TEST(MockEngine, InsertAndGetSingleRow) {
    MockStorageEngine engine;

    engine.create_table("users", make_schema());
    engine.insert("users", 1, {"Alice", 25});

    auto cursor = engine.get("users", 1, {0, 1});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);

    EXPECT_EQ(std::get<std::string>(*cursor->value(0)), "Alice");
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 25);

    cursor->next();
    EXPECT_FALSE(cursor->valid());
}

TEST(MockEngine, InsertOverwritesSameKey) {
    MockStorageEngine engine;

    engine.create_table("users", make_schema());

    engine.insert("users", 1, {"A", 10});
    engine.insert("users", 1, {"B", 20});

    auto cursor = engine.get("users", 1, {0, 1});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(std::get<std::string>(*cursor->value(0)), "B");
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 20);
}

TEST(MockEngine, GetMissingKeyIsEmpty) {
    MockStorageEngine engine;

    engine.create_table("users", make_schema());
    engine.insert("users", 1, {"A", 10});

    auto cursor = engine.get("users", 999, {0, 1});

    EXPECT_FALSE(cursor->valid());
}

TEST(MockEngine, ScanRangeFiltering) {
    MockStorageEngine engine;

    engine.create_table("users", make_schema());

    engine.insert("users", 1, {"A", 10});
    engine.insert("users", 2, {"B", 20});
    engine.insert("users", 3, {"C", 30});

    auto cursor = engine.scan("users", 2, 3, {0});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 2);

    cursor->next();
    EXPECT_FALSE(cursor->valid());
}

TEST(MockEngine, ScanFullRangeSortedOrder) {
    MockStorageEngine engine;

    engine.create_table("users", make_schema());

    engine.insert("users", 3, {"C", 30});
    engine.insert("users", 1, {"A", 10});
    engine.insert("users", 2, {"B", 20});

    auto cursor = engine.scan("users", std::nullopt, std::nullopt, {});

    std::vector<Key> got;

    while (cursor->valid()) {
        got.push_back(cursor->key());
        cursor->next();
    }

    EXPECT_EQ(got, std::vector<Key>({1, 2, 3}));
}

TEST(MockEngine, TableIsolation) {
    MockStorageEngine engine;

    engine.create_table("a", make_schema());
    engine.create_table("b", make_schema());

    engine.insert("a", 1, {"A", 10});
    engine.insert("b", 1, {"B", 20});

    auto ca = engine.get("a", 1, {0});
    auto cb = engine.get("b", 1, {0});

    ASSERT_TRUE(ca->valid());
    ASSERT_TRUE(cb->valid());

    EXPECT_EQ(std::get<std::string>(*ca->value(0)), "A");
    EXPECT_EQ(std::get<std::string>(*cb->value(0)), "B");
}

TEST(MockEngine, InvalidRowSizeThrows) {
    MockStorageEngine engine;

    engine.create_table("users", make_schema());

    Row bad = {"A", 10};
    bad.pop_back(); // ломаем schema

    EXPECT_THROW(engine.insert("users", 1, bad), std::runtime_error);
}

TEST(MockEngine, NullableFieldWorks) {
    MockStorageEngine engine;

    engine.create_table("users", make_schema());

    engine.insert("users", 1, {"Alice", std::nullopt});

    auto cursor = engine.get("users", 1, {0, 1});

    ASSERT_TRUE(cursor->valid());
    EXPECT_FALSE(cursor->value(1).has_value());
}

TEST(MockEngine, GetAndScanDoNotInterfere) {
    MockStorageEngine engine;

    engine.create_table("users", make_schema());

    engine.insert("users", 1, {"A", 10});
    engine.insert("users", 2, {"B", 20});

    auto c1 = engine.get("users", 1, {0});
    auto c2 = engine.scan("users", std::nullopt, std::nullopt, {0});

    ASSERT_TRUE(c1->valid());
    ASSERT_TRUE(c2->valid());

    EXPECT_EQ(c1->key(), 1);
    EXPECT_EQ(c2->key(), 1);
}
