#include <gtest/gtest.h>

#include "storage/model/schema.hpp"

using namespace htap::storage;

TEST(SchemaTest, AddsColumnsCorrectly) {
    Schema schema;

    schema.add_column("key", ValueType::INT64, true, false);
    schema.add_column("name", ValueType::STRING, false, true);

    EXPECT_EQ(schema.size(), 2u);

    const auto& col0 = schema.get_column(0);
    EXPECT_EQ(col0.name, "key");
    EXPECT_EQ(col0.type, ValueType::INT64);
    EXPECT_TRUE(col0.is_key);
    EXPECT_FALSE(col0.nullable);

    const auto& col1 = schema.get_column(1);
    EXPECT_EQ(col1.name, "name");
    EXPECT_EQ(col1.type, ValueType::STRING);
    EXPECT_FALSE(col1.is_key);
    EXPECT_TRUE(col1.nullable);
}

TEST(SchemaTest, FindsColumnIndexByName) {
    Schema schema;

    schema.add_column("key", ValueType::INT64, true);
    schema.add_column("age", ValueType::INT64);

    auto idx = schema.get_column_index("age");
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(idx.value(), 1u);
}

TEST(SchemaTest, ReturnsNulloptForUnknownColumn) {
    Schema schema;

    schema.add_column("key", ValueType::INT64, true);

    auto idx = schema.get_column_index("unknown");
    EXPECT_FALSE(idx.has_value());
}

TEST(SchemaTest, IdentifiesKeyColumn) {
    Schema schema;

    schema.add_column("key", ValueType::INT64, true);
    schema.add_column("name", ValueType::STRING);

    EXPECT_EQ(schema.key_column_index(), 0u);
}

TEST(SchemaTest, ThrowsOnMultipleKeyColumns) {
    Schema schema;

    schema.add_column("key1", ValueType::INT64, true);

    EXPECT_THROW(
        schema.add_column("key2", ValueType::INT64, true),
        std::invalid_argument
    );
}

TEST(SchemaTest, ValidatesNotNullConstraint) {
    Schema schema;

    schema.add_column("key", ValueType::INT64, true, false);

    NullableValue null_value = std::nullopt;
    NullableValue valid_value = int64_t(10);

    EXPECT_FALSE(schema.is_valid_value(0, null_value));
    EXPECT_TRUE(schema.is_valid_value(0, valid_value));
}

TEST(SchemaTest, ValidatesTypeMatching) {
    Schema schema;

    schema.add_column("age", ValueType::INT64);

    NullableValue wrong_type = std::string("hello");

    EXPECT_FALSE(schema.is_valid_value(0, wrong_type));
}

TEST(SchemaTest, AllowsNullForNullableColumn) {
    Schema schema;

    schema.add_column("name", ValueType::STRING, false, true);

    NullableValue null_value = std::nullopt;

    EXPECT_TRUE(schema.is_valid_value(0, null_value));
}
