#include <gtest/gtest.h>

#include "storage/mock/mock_storage_engine.hpp"
#include "storage/model/schema_builder.hpp"

using namespace htap::storage;

// -------------------- helpers --------------------

static Schema make_schema() {
    return SchemaBuilder()
        .add_column("id", ValueType::INT64, true, false)
        .add_column("name", ValueType::STRING, false, true)
        .add_column("age", ValueType::INT64, false, true)
        .build();
}

static Row make_row(int64_t id, std::string name, std::optional<int64_t> age) {
    Row r(3);

    r[0] = Value{id};
    r[1] = name;
    if (age.has_value())
        r[2] = Value{*age};
    else
        r[2] = std::nullopt;

    return r;
}

// -------------------- CURSOR TESTS --------------------

TEST(MockCursor, EmptyScanIsInvalid) {
    MockStorageEngine engine;
    engine.create_table("t", make_schema());

    auto cursor = engine.scan("t", 1, 10, {0});

    EXPECT_FALSE(cursor->valid());
}

TEST(MockCursor, SingleRowValidIteration) {
    MockStorageEngine engine;
    engine.create_table("t", make_schema());

    engine.insert("t", make_row(1, "A", 10));

    auto cursor = engine.scan("t", 1, 2, {0,1,2});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);

    EXPECT_EQ(std::get<int64_t>(*cursor->value(0)), 1);
    EXPECT_EQ(std::get<std::string>(*cursor->value(1)), "A");
    EXPECT_EQ(std::get<int64_t>(*cursor->value(2)), 10);

    cursor->next();
    EXPECT_FALSE(cursor->valid());
}

TEST(MockCursor, FullIterationSortedOrder) {
    MockStorageEngine engine;
    engine.create_table("t", make_schema());

    engine.insert("t", make_row(3, "C", 30));
    engine.insert("t", make_row(1, "A", 10));
    engine.insert("t", make_row(2, "B", 20));

    auto cursor = engine.scan("t", std::nullopt, std::nullopt, {0});

    std::vector<Key> got;

    while (cursor->valid()) {
        got.push_back(cursor->key());
        cursor->next();
    }

    EXPECT_EQ(got, std::vector<Key>({1, 2, 3}));
}

TEST(MockCursor, ProjectionEnforced) {
    MockStorageEngine engine;
    engine.create_table("t", make_schema());

    engine.insert("t", make_row(1, "A", 10));

    auto cursor = engine.scan("t", std::nullopt, std::nullopt, {0}); // only id

    ASSERT_TRUE(cursor->valid());

    EXPECT_EQ(std::get<int64_t>(*cursor->value(0)), 1);

    EXPECT_THROW(cursor->value(1), std::runtime_error);
}
