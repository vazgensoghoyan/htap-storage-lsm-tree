#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "lsmtree/sstable/build/column_sst_block_builder.hpp"
#include "lsmtree/sstable/build/row_sst_block_builder.hpp"
#include "storage/model/schema_builder.hpp"

using namespace htap::lsmtree::sstable;
using namespace htap::storage;

namespace {

Schema make_schema() {
    return SchemaBuilder()
        .add_column("id", ValueType::INT64, true, false)
        .add_column("age", ValueType::INT64, false, true)
        .add_column("score", ValueType::DOUBLE, false, true)
        .add_column("name", ValueType::STRING, false, true)
        .build();
}

Row make_row(
    std::int64_t id,
    std::optional<std::int64_t> age,
    std::optional<double> score,
    std::optional<std::string> name = std::string("user")
) {
    Row row;
    row.push_back(id);

    if (age.has_value()) {
        row.push_back(*age);
    } else {
        row.push_back(std::nullopt);
    }

    if (score.has_value()) {
        row.push_back(*score);
    } else {
        row.push_back(std::nullopt);
    }

    if (name.has_value()) {
        row.push_back(*name);
    } else {
        row.push_back(std::nullopt);
    }

    return row;
}

const htap::storage::read::sstable::NumericBlockStats* find_stats(
    const std::vector<htap::storage::read::sstable::NumericBlockStats>& stats,
    std::size_t column_idx
) {
    for (const auto& entry : stats) {
        if (entry.column_idx == column_idx) {
            return &entry;
        }
    }

    return nullptr;
}

}

TEST(NumericStatsBuilderTest, RowBuilderComputesInt64AndDoubleStats) {
    const auto schema = make_schema();
    RowSSTBlockBuilder builder(schema);

    builder.add(make_row(1, 10, 1.5));
    builder.add(make_row(2, 20, 3.0));
    builder.add(make_row(3, std::nullopt, std::nullopt));
    builder.add(make_row(4, 5, 2.0));

    const auto result = builder.finish();

    ASSERT_EQ(result.numeric_stats.size(), 2u);

    const auto* age = find_stats(result.numeric_stats, 1);
    ASSERT_NE(age, nullptr);
    EXPECT_EQ(age->type, ValueType::INT64);
    EXPECT_TRUE(age->has_value);
    EXPECT_EQ(std::get<std::int64_t>(age->min_value), 5);
    EXPECT_EQ(std::get<std::int64_t>(age->max_value), 20);

    const auto* score = find_stats(result.numeric_stats, 2);
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->type, ValueType::DOUBLE);
    EXPECT_TRUE(score->has_value);
    EXPECT_DOUBLE_EQ(std::get<double>(score->min_value), 1.5);
    EXPECT_DOUBLE_EQ(std::get<double>(score->max_value), 3.0);
}

TEST(NumericStatsBuilderTest, RowBuilderIgnoresKeyStringsNullsAndNaN) {
    const auto schema = make_schema();
    RowSSTBlockBuilder builder(schema);

    builder.add(make_row(1, std::nullopt, std::numeric_limits<double>::quiet_NaN()));
    builder.add(make_row(2, std::nullopt, std::nullopt));

    const auto result = builder.finish();

    EXPECT_EQ(find_stats(result.numeric_stats, 0), nullptr);
    EXPECT_EQ(find_stats(result.numeric_stats, 3), nullptr);

    const auto* age = find_stats(result.numeric_stats, 1);
    ASSERT_NE(age, nullptr);
    EXPECT_FALSE(age->has_value);

    const auto* score = find_stats(result.numeric_stats, 2);
    ASSERT_NE(score, nullptr);
    EXPECT_FALSE(score->has_value);
}

TEST(NumericStatsBuilderTest, ColumnBuilderComputesStatsForNumericColumn) {
    const auto schema = make_schema();
    ColumnSSTBlockBuilder builder(schema.get_column(1), 1);

    builder.add(make_row(1, 10, 1.0));
    builder.add(make_row(2, std::nullopt, 2.0));
    builder.add(make_row(3, 5, 3.0));

    const auto result = builder.finish();

    ASSERT_EQ(result.numeric_stats.size(), 1u);
    const auto& age = result.numeric_stats.front();
    EXPECT_EQ(age.column_idx, 1u);
    EXPECT_EQ(age.type, ValueType::INT64);
    EXPECT_TRUE(age.has_value);
    EXPECT_EQ(std::get<std::int64_t>(age.min_value), 5);
    EXPECT_EQ(std::get<std::int64_t>(age.max_value), 10);
}

TEST(NumericStatsBuilderTest, ColumnBuilderSkipsStringAndKeyColumns) {
    const auto schema = make_schema();

    ColumnSSTBlockBuilder key_builder(schema.get_column(0), 0);
    key_builder.add(make_row(1, 10, 1.0));
    EXPECT_TRUE(key_builder.finish().numeric_stats.empty());

    ColumnSSTBlockBuilder string_builder(schema.get_column(3), 3);
    string_builder.add(make_row(1, 10, 1.0, std::string("Ann")));
    EXPECT_TRUE(string_builder.finish().numeric_stats.empty());
}
