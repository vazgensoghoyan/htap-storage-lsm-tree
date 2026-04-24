#include <gtest/gtest.h>

#include "storage/model/schema_builder.hpp"

using namespace htap::storage;

TEST(SchemaBuilderTest, BuildsValidSchema) {
    Schema schema = SchemaBuilder()
        .add_column("id", ValueType::INT64, true, false)
        .add_column("name", ValueType::STRING, false, true)
        .build();

    EXPECT_EQ(schema.size(), 2u);

    const auto& col0 = schema.get_column(0);
    EXPECT_EQ(col0.name, "id");
    EXPECT_TRUE(col0.is_key);
    EXPECT_FALSE(col0.nullable);

    const auto& col1 = schema.get_column(1);
    EXPECT_EQ(col1.name, "name");
    EXPECT_FALSE(col1.is_key);
    EXPECT_TRUE(col1.nullable);

    EXPECT_EQ(schema.key_column_index(), 0u);
}

TEST(SchemaBuilderTest, ThrowsIfNoColumns) {
    SchemaBuilder builder;

    EXPECT_THROW(builder.build(), std::runtime_error);
}

TEST(SchemaBuilderTest, ThrowsIfNoPrimaryKey) {
    SchemaBuilder builder;

    builder.add_column("name", ValueType::STRING);

    EXPECT_THROW(builder.build(), std::runtime_error);
}

TEST(SchemaBuilderTest, ThrowsIfMultiplePrimaryKeys) {
    SchemaBuilder builder;

    builder.add_column("id1", ValueType::INT64, true, false);
    builder.add_column("id2", ValueType::INT64, true, false);

    EXPECT_THROW(builder.build(), std::runtime_error);
}

TEST(SchemaBuilderTest, ThrowsIfKeyNotInt64) {
    SchemaBuilder builder;

    builder.add_column("id", ValueType::STRING, true, false);

    EXPECT_THROW(builder.build(), std::runtime_error);
}

TEST(SchemaBuilderTest, ThrowsIfKeyNullable) {
    SchemaBuilder builder;

    builder.add_column("id", ValueType::INT64, true, true);

    EXPECT_THROW(builder.build(), std::runtime_error);
}

TEST(SchemaBuilderTest, ThrowsOnDuplicateColumnNames) {
    SchemaBuilder builder;

    builder.add_column("id", ValueType::INT64, true, false);

    EXPECT_THROW(
        builder.add_column("id", ValueType::STRING),
        std::invalid_argument
    );
}
