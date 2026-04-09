#include <gtest/gtest.h>

#include "storage/mock/mock_storage_engine.hpp"

using namespace htap::storage;

namespace {

Schema make_schema() {
    Schema schema;
    schema.add_column("id", ValueType::INT64, true, false);
    schema.add_column("name", ValueType::STRING, false, true);
    return schema;
}

} // namespace

TEST(MockStorageEngineTest, InsertAndGetWorks) {
    auto schema = make_schema();
    MockStorageEngine storage(schema);

    storage.insert(1, {int64_t(10), std::string("abc")});

    auto cursor = storage.get(1, {0, 1});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);
    EXPECT_EQ(std::get<int64_t>(cursor->value(0)), 10);
    EXPECT_EQ(std::get<std::string>(cursor->value(1)), "abc");
}

TEST(MockStorageEngineTest, GetReturnsEmptyCursorIfNotFound) {
    auto schema = make_schema();
    MockStorageEngine storage(schema);

    auto cursor = storage.get(999, {0});

    EXPECT_FALSE(cursor->valid());
}

TEST(MockStorageEngineTest, ProjectionWorks) {
    auto schema = make_schema();
    MockStorageEngine storage(schema);

    storage.insert(1, {int64_t(10), std::string("abc")});

    auto cursor = storage.get(1, {1});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(std::get<std::string>(cursor->value(0)), "abc");
}

TEST(MockStorageEngineTest, ScanRangeWorks) {
    auto schema = make_schema();
    MockStorageEngine storage(schema);

    storage.insert(1, {int64_t(10), std::string("a")});
    storage.insert(2, {int64_t(20), std::string("b")});
    storage.insert(3, {int64_t(30), std::string("c")});

    auto cursor = storage.scan(1, 2, {0});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);

    cursor->next();
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 2);

    cursor->next();
    EXPECT_FALSE(cursor->valid());
}

TEST(MockStorageEngineTest, ScanReturnsEmptyIfNoMatches) {
    auto schema = make_schema();
    MockStorageEngine storage(schema);

    storage.insert(10, {int64_t(100), std::string("x")});

    auto cursor = storage.scan(1, 5, {0});

    EXPECT_FALSE(cursor->valid());
}

TEST(MockStorageEngineTest, InsertValidatesSchemaSize) {
    auto schema = make_schema();
    MockStorageEngine storage(schema);

    EXPECT_THROW(
        storage.insert(1, {int64_t(10)}), // не хватает колонки
        std::invalid_argument
    );
}

TEST(MockStorageEngineTest, InsertValidatesType) {
    auto schema = make_schema();
    MockStorageEngine storage(schema);

    EXPECT_THROW(
        storage.insert(1, {int64_t(10), int64_t(20)}), // строка ожидается
        std::invalid_argument
    );
}

TEST(MockStorageEngineTest, InsertValidatesNullability) {
    auto schema = make_schema();
    MockStorageEngine storage(schema);

    EXPECT_THROW(
        storage.insert(1, {std::nullopt, std::string("abc")}),
        std::invalid_argument
    );
}

TEST(MockStorageEngineTest, ThrowsOnDuplicateKey) {
    auto schema = make_schema();
    MockStorageEngine storage(schema);

    storage.insert(1, {int64_t(10), std::string("a")});

    EXPECT_THROW(
        storage.insert(1, {int64_t(20), std::string("b")}),
        std::runtime_error
    );
}
