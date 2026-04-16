#include <gtest/gtest.h>

#include "storage/mock/mock_storage_engine.hpp"
#include "storage/model/schema_builder.hpp"

using namespace htap::storage;

class MockStorageEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine.create_table("users", schema);
    }

    MockStorageEngine engine;

    Schema schema = SchemaBuilder()
        .add_column("id", ValueType::INT64, true, false)
        .add_column("name", ValueType::STRING, false, true)
        .add_column("score", ValueType::DOUBLE, false, true)
        .build();
};

TEST_F(MockStorageEngineTest, CreateTableAndExists) {
    EXPECT_TRUE(engine.table_exists("users"));
    EXPECT_FALSE(engine.table_exists("missing"));
}

TEST_F(MockStorageEngineTest, DuplicateTableThrows) {
    EXPECT_THROW(
        engine.create_table("users", schema),
        std::runtime_error
    );
}

TEST_F(MockStorageEngineTest, GetSchema) {
    const Schema& s = engine.get_table_schema("users");

    EXPECT_EQ(s.size(), 3);
    EXPECT_EQ(s.key_column_index(), 0);
}

TEST_F(MockStorageEngineTest, InsertAndGet) {
    engine.insert("users", {
        Value(int64_t(1)),
        Value(std::string("alice")),
        Value(42.5)
    });

    auto row_opt = engine.get("users", 1, {0, 1, 2});

    ASSERT_TRUE(row_opt.has_value());

    const Row& row = row_opt.value();

    EXPECT_EQ(std::get<int64_t>(row[0].value()), 1);
    EXPECT_EQ(std::get<std::string>(row[1].value()), "alice");
    EXPECT_EQ(std::get<double>(row[2].value()), 42.5);
}

TEST_F(MockStorageEngineTest, GetMissingKey) {
    auto row_opt = engine.get("users", 999, {0, 1, 2});

    EXPECT_FALSE(row_opt.has_value());
}

TEST_F(MockStorageEngineTest, InsertNullsAllowed) {
    engine.insert("users", {
        Value(int64_t(2)),
        std::nullopt,
        Value(10.0)
    });

    auto row_opt = engine.get("users", 2, {0, 1, 2});
    ASSERT_TRUE(row_opt.has_value());

    const Row& row = row_opt.value();

    ASSERT_TRUE(row[0].has_value());
    EXPECT_EQ(std::get<int64_t>(row[0].value()), 2);

    EXPECT_FALSE(row[1].has_value());

    ASSERT_TRUE(row[2].has_value());
    EXPECT_EQ(std::get<double>(row[2].value()), 10.0);
}

TEST_F(MockStorageEngineTest, InsertInvalidRowSize) {
    EXPECT_THROW(
        engine.insert("users", {
            Value(int64_t(1)),
            Value(std::string("bad"))
        }),
        std::runtime_error
    );
}

TEST_F(MockStorageEngineTest, InsertNullKeyThrows) {
    EXPECT_THROW(
        engine.insert("users", {
            std::nullopt,
            Value(std::string("alice")),
            Value(1.0)
        }),
        std::runtime_error
    );
}

TEST_F(MockStorageEngineTest, InsertWrongKeyTypeThrows) {
    EXPECT_THROW(
        engine.insert("users", {
            Value(std::string("not_int")),
            Value(std::string("alice")),
            Value(1.0)
        }),
        std::runtime_error
    );
}

TEST_F(MockStorageEngineTest, InsertOverwrite) {
    engine.insert("users", {
        Value(int64_t(1)),
        Value(std::string("alice")),
        Value(10.0)
    });

    engine.insert("users", {
        Value(int64_t(1)),
        Value(std::string("alice_updated")),
        Value(99.9)
    });

    auto row_opt = engine.get("users", 1, {0, 1, 2});
    ASSERT_TRUE(row_opt.has_value());

    const Row& row = row_opt.value();

    EXPECT_EQ(std::get<std::string>(row[1].value()), "alice_updated");
    EXPECT_EQ(std::get<double>(row[2].value()), 99.9);
}

TEST_F(MockStorageEngineTest, ScanFullRange) {
    engine.insert("users", {Value(int64_t(1)), Value("a"), Value(1.0)});
    engine.insert("users", {Value(int64_t(3)), Value("c"), Value(3.0)});
    engine.insert("users", {Value(int64_t(2)), Value("b"), Value(2.0)});

    auto cursor = engine.scan("users", std::nullopt, std::nullopt, {0, 1, 2});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);

    cursor->next();
    EXPECT_EQ(cursor->key(), 2);

    cursor->next();
    EXPECT_EQ(cursor->key(), 3);

    cursor->next();
    EXPECT_FALSE(cursor->valid());
}

TEST_F(MockStorageEngineTest, ScanRangeExclusive) {
    engine.insert("users", {Value(int64_t(1)), Value("a"), Value(1.0)});
    engine.insert("users", {Value(int64_t(2)), Value("b"), Value(2.0)});
    engine.insert("users", {Value(int64_t(3)), Value("c"), Value(3.0)});

    auto cursor = engine.scan("users", 2, 3, {0, 1, 2});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 2);

    cursor->next();
    EXPECT_FALSE(cursor->valid());
}

TEST_F(MockStorageEngineTest, ProjectionWorks) {
    engine.insert("users", {
        Value(int64_t(1)),
        Value("alice"),
        Value(42.0)
    });

    auto row_opt = engine.get("users", 1, {0, 2});

    ASSERT_TRUE(row_opt.has_value());

    const Row& row = row_opt.value();

    EXPECT_EQ(std::get<int64_t>(row[0].value()), 1);
    EXPECT_EQ(std::get<double>(row[1].value()), 42.0);
}

TEST_F(MockStorageEngineTest, TableNotFoundThrows) {
    EXPECT_THROW(
        engine.insert("bad", {Value(int64_t(1))}),
        std::runtime_error
    );

    EXPECT_THROW(
        engine.get("bad", 1, {0}),
        std::runtime_error
    );

    EXPECT_THROW(
        engine.scan("bad", std::nullopt, std::nullopt, {0}),
        std::runtime_error
    );
}
