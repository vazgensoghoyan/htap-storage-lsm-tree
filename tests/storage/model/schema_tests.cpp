#include <gtest/gtest.h>

#include "storage/model/schema.hpp"
#include "storage/model/schema_builder.hpp"

using namespace htap::storage;

static Schema make_schema() {
    return SchemaBuilder()
        .add_column("id", ValueType::INT64, true, false)
        .add_column("name", ValueType::STRING, false, true)
        .add_column("score", ValueType::DOUBLE, false, true)
        .build();
}

TEST(SchemaTest, SizeIsCorrect) {
    Schema schema = make_schema();
    EXPECT_EQ(schema.size(), 3);
}

TEST(SchemaTest, GetColumnValid) {
    Schema schema = make_schema();

    const auto& col = schema.get_column(1);

    EXPECT_EQ(col.name, "name");
    EXPECT_EQ(col.type, ValueType::STRING);
}

TEST(SchemaTest, GetColumnOutOfRangeThrows) {
    Schema schema = make_schema();

    EXPECT_THROW(schema.get_column(100), std::out_of_range);
}

TEST(SchemaTest, ValidIntValue) {
    Schema schema = make_schema();

    NullableValue v = int64_t(42);

    EXPECT_TRUE(schema.is_valid_value(0, v)); // id
}

TEST(SchemaTest, ValidStringValue) {
    Schema schema = make_schema();

    NullableValue v = std::string("hello");

    EXPECT_TRUE(schema.is_valid_value(1, v));
}

TEST(SchemaTest, ValidDoubleValue) {
    Schema schema = make_schema();

    NullableValue v = 3.14;

    EXPECT_TRUE(schema.is_valid_value(2, v));
}

TEST(SchemaTest, InvalidTypeMismatch) {
    Schema schema = make_schema();

    NullableValue v = std::string("not int");

    EXPECT_FALSE(schema.is_valid_value(0, v));
}

TEST(SchemaTest, NullAllowedColumn) {
    Schema schema = make_schema();

    NullableValue v = std::nullopt;

    EXPECT_TRUE(schema.is_valid_value(1, v)); // name nullable
}

TEST(SchemaTest, NullNotAllowedForKey) {
    Schema schema = make_schema();

    NullableValue v = std::nullopt;

    EXPECT_FALSE(schema.is_valid_value(0, v)); // id not nullable
}
