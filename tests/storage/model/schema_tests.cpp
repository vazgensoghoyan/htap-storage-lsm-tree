#include <gtest/gtest.h>

#include "storage/model/schema_builder.hpp"

using namespace htap::storage;

// -------------------- helper --------------------

static Schema make_schema() {
    return SchemaBuilder()
        .add_column("id", ValueType::INT64, false)
        .add_column("age", ValueType::INT64, true)
        .add_column("name", ValueType::STRING, true)
        .build();
}

// -------------------- SCHEMA TESTS --------------------

TEST(SchemaTest, ReturnsCorrectSize) {
    auto schema = make_schema();
    EXPECT_EQ(schema.size(), 3u);
}

TEST(SchemaTest, GetColumnByIndex) {
    auto schema = make_schema();

    const auto& col = schema.get_column(1);

    EXPECT_EQ(col.name, "age");
    EXPECT_EQ(col.type, ValueType::INT64);
}

TEST(SchemaTest, ThrowsOnInvalidColumnIndex) {
    auto schema = make_schema();

    EXPECT_THROW(schema.get_column(100), std::out_of_range);
}

TEST(SchemaTest, FindsColumnIndexByName) {
    auto schema = make_schema();

    auto idx = schema.get_column_index("name");

    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(idx.value(), 2u);
}

TEST(SchemaTest, ReturnsNulloptForUnknownColumn) {
    auto schema = make_schema();

    auto idx = schema.get_column_index("unknown");

    EXPECT_FALSE(idx.has_value());
}

TEST(SchemaTest, ValidatesTypeMatching) {
    auto schema = make_schema();

    auto idx = schema.get_column_index("age").value();

    NullableValue wrong_type = std::string("oops");

    EXPECT_FALSE(schema.is_valid_value(idx, wrong_type));
}

TEST(SchemaTest, AllowsNullForNullableColumn) {
    auto schema = make_schema();

    auto idx = schema.get_column_index("name").value();

    NullableValue null_value = std::nullopt;

    EXPECT_TRUE(schema.is_valid_value(idx, null_value));
}

TEST(SchemaTest, AcceptsCorrectValues) {
    auto schema = make_schema();

    auto idx = schema.get_column_index("age").value();

    NullableValue value = int64_t(30);

    EXPECT_TRUE(schema.is_valid_value(idx, value));
}
