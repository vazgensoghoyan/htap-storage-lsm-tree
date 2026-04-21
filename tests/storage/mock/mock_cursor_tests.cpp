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

    engine.insert("t", 1, {"A", 10});

    auto cursor = engine.scan("t", 1, 2, {0,1});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);
    EXPECT_EQ(std::get<std::string>(*cursor->value(0)), "A");
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 10);

    cursor->next();
    EXPECT_FALSE(cursor->valid());
}

TEST(MockCursor, FullIterationSortedOrder) {
    MockStorageEngine engine;
    engine.create_table("t", make_schema());

    engine.insert("t", 3, {"C", 30});
    engine.insert("t", 1, {"A", 10});
    engine.insert("t", 2, {"B", 20});

    auto cursor = engine.scan("t", std::nullopt, std::nullopt, {});

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

    engine.insert("t", 1, {"A", 10});

    auto cursor = engine.scan("t", std::nullopt, std::nullopt, {});

    ASSERT_TRUE(cursor->valid());

    EXPECT_EQ(cursor->key(), 1);

    EXPECT_THROW(cursor->value(1), std::runtime_error);
}
