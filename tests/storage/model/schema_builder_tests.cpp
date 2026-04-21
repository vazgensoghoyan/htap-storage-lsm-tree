#include <gtest/gtest.h>

#include "storage/model/schema_builder.hpp"

using namespace htap::storage;

TEST(SchemaBuilderTest, BuildsValidSchema) {
    Schema schema = SchemaBuilder()
        .add_column("id", ValueType::INT64, false)
        .add_column("name", ValueType::STRING, true)
        .build();

    EXPECT_EQ(schema.size(), 2u);

    const auto& col0 = schema.get_column(0);
    EXPECT_EQ(col0.name, "id");
    EXPECT_FALSE(col0.nullable);

    const auto& col1 = schema.get_column(1);
    EXPECT_EQ(col1.name, "name");
    EXPECT_TRUE(col1.nullable);
}

TEST(SchemaBuilderTest, ThrowsIfNoColumns) {
    SchemaBuilder builder;

    EXPECT_THROW(builder.build(), std::runtime_error);
}

TEST(SchemaBuilderTest, ThrowsOnDuplicateColumnNames) {
    SchemaBuilder builder;

    builder.add_column("id", ValueType::INT64, false);

    EXPECT_THROW(
        builder.add_column("id", ValueType::STRING, true),
        std::invalid_argument
    );
}
