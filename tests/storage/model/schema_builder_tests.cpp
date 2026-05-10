#include <gtest/gtest.h>

#include "storage/model/schema_builder.hpp"

using namespace htap::storage;

TEST(SchemaBuilderTest, BuildsValidSchema) {
    Schema schema = SchemaBuilder()
        .add_column("id", ValueType::INT64, true, false)
        .add_column("name", ValueType::STRING, false, true)
        .build();

    EXPECT_EQ(schema.size(), 2);

    const auto& cols = schema.columns();
    EXPECT_TRUE(cols[KEY_COLUMN_INDEX].is_key);
    EXPECT_EQ(cols[KEY_COLUMN_INDEX].name, "id");
    EXPECT_EQ(cols[KEY_COLUMN_INDEX].type, ValueType::INT64);

    EXPECT_EQ(cols[1].name, "name");
}

TEST(SchemaBuilderTest, ThrowsOnDuplicateColumnNames) {
    EXPECT_THROW(
        {
            SchemaBuilder builder;
            builder.add_column("id", ValueType::INT64, true, false);
            builder.add_column("id", ValueType::STRING);
        },
        std::invalid_argument
    );
}

TEST(SchemaBuilderTest, ThrowsIfNoKey) {
    EXPECT_THROW(
        {
            SchemaBuilder builder;
            builder.add_column("name", ValueType::STRING);
            builder.build();
        },
        std::runtime_error
    );
}

TEST(SchemaBuilderTest, ThrowsIfKeyNotAtIndexZero) {
    EXPECT_THROW(
        {
            SchemaBuilder builder;
            builder.add_column("name", ValueType::STRING);
            builder.add_column("id", ValueType::INT64, true, false);
        },
        std::runtime_error
    );
}

TEST(SchemaBuilderTest, ThrowsOnMultipleKeys) {
    EXPECT_THROW(
        {
            SchemaBuilder builder;
            builder.add_column("id1", ValueType::INT64, true, false);
            builder.add_column("id2", ValueType::INT64, true, false);
        },
        std::runtime_error
    );
}

TEST(SchemaBuilderTest, ThrowsIfKeyNotInt64) {
    EXPECT_THROW(
        {
            SchemaBuilder builder;
            builder.add_column("id", ValueType::STRING, true, false);
        },
        std::runtime_error
    );
}

TEST(SchemaBuilderTest, ThrowsIfKeyNullable) {
    EXPECT_THROW(
        {
            SchemaBuilder builder;
            builder.add_column("id", ValueType::INT64, true, true);
        },
        std::runtime_error
    );
}
