#include <gtest/gtest.h>

#include "storage/read/data_skipping_filter.hpp"
#include "storage/read/sstable/data_skipping_block_pruner.hpp"

using namespace htap::storage;
using namespace htap::storage::read;
using namespace htap::storage::read::sstable;

namespace {

NumericBlockStats make_age_stats(std::int64_t min, std::int64_t max) {
    return NumericBlockStats{
        .column_idx = 1,
        .type = ValueType::INT64,
        .has_value = true,
        .min_value = min,
        .max_value = max
    };
}

NumericStatsRange make_stats_range() {
    NumericStatsRange range;
    range.first_block_id = 0;
    range.block_count = 3;
    range.by_column.emplace(1, std::vector<NumericBlockStats>{
        make_age_stats(0, 10),
        make_age_stats(20, 30),
        make_age_stats(40, 50)
    });
    return range;
}

std::vector<RowBlockMeta> make_blocks() {
    return {
        RowBlockMeta{.min_key = 0, .max_key = 9, .offset = 0, .size = 1, .row_count = 1, .block_id = 0},
        RowBlockMeta{.min_key = 10, .max_key = 19, .offset = 1, .size = 1, .row_count = 1, .block_id = 1},
        RowBlockMeta{.min_key = 20, .max_key = 29, .offset = 2, .size = 1, .row_count = 1, .block_id = 2}
    };
}

DataSkippingFilter filter(NumericComparisonOp op, std::int64_t value) {
    DataSkippingFilter result;
    result.predicates.push_back(NumericColumnPredicate{
        .column_idx = 1,
        .op = op,
        .value = value
    });
    return result;
}

std::vector<std::size_t> block_ids(const std::vector<RowBlockMeta>& blocks) {
    std::vector<std::size_t> result;
    for (const auto& block : blocks) {
        result.push_back(block.block_id);
    }
    return result;
}

}

TEST(DataSkippingBlockPrunerTest, AppliesBasicComparisonRules) {
    const DataSkippingBlockPruner pruner;
    const auto stats = make_stats_range();
    const auto blocks = make_blocks();

    EXPECT_EQ(block_ids(pruner.filter_row_blocks(
        blocks,
        stats,
        filter(NumericComparisonOp::Greater, 25)
    )), (std::vector<std::size_t>{1, 2}));

    EXPECT_EQ(block_ids(pruner.filter_row_blocks(
        blocks,
        stats,
        filter(NumericComparisonOp::Less, 15)
    )), (std::vector<std::size_t>{0}));

    EXPECT_EQ(block_ids(pruner.filter_row_blocks(
        blocks,
        stats,
        filter(NumericComparisonOp::Equal, 25)
    )), (std::vector<std::size_t>{1}));

    EXPECT_TRUE(pruner.filter_row_blocks(
        blocks,
        stats,
        filter(NumericComparisonOp::GreaterEqual, 60)
    ).empty());
}

TEST(DataSkippingBlockPrunerTest, AppliesAndPredicates) {
    const DataSkippingBlockPruner pruner;
    const auto stats = make_stats_range();
    const auto blocks = make_blocks();

    DataSkippingFilter range_filter;
    range_filter.predicates.push_back(NumericColumnPredicate{
        .column_idx = 1,
        .op = NumericComparisonOp::GreaterEqual,
        .value = std::int64_t{20}
    });
    range_filter.predicates.push_back(NumericColumnPredicate{
        .column_idx = 1,
        .op = NumericComparisonOp::Less,
        .value = std::int64_t{40}
    });

    EXPECT_EQ(
        block_ids(pruner.filter_row_blocks(blocks, stats, range_filter)),
        (std::vector<std::size_t>{1})
    );
}

TEST(DataSkippingBlockPrunerTest, MissingColumnStatsAreConservative) {
    const DataSkippingBlockPruner pruner;
    const auto blocks = make_blocks();
    NumericStatsRange stats;
    stats.first_block_id = 0;
    stats.block_count = 3;

    EXPECT_EQ(
        block_ids(pruner.filter_row_blocks(blocks, stats, filter(NumericComparisonOp::Greater, 100))),
        (std::vector<std::size_t>{0, 1, 2})
    );
}

TEST(DataSkippingBlockPrunerTest, KeepsInt64ComparisonsExactForLargeValues) {
    const DataSkippingBlockPruner pruner;

    NumericStatsRange stats;
    stats.first_block_id = 0;
    stats.block_count = 1;
    stats.by_column.emplace(1, std::vector<NumericBlockStats>{
        make_age_stats(9'007'199'254'740'993LL, 9'007'199'254'740'993LL)
    });

    const std::vector<RowBlockMeta> blocks{
        RowBlockMeta{.min_key = 0, .max_key = 0, .offset = 0, .size = 1, .row_count = 1, .block_id = 0}
    };

    EXPECT_EQ(
        block_ids(pruner.filter_row_blocks(
            blocks,
            stats,
            filter(NumericComparisonOp::Greater, 9'007'199'254'740'992LL)
        )),
        (std::vector<std::size_t>{0})
    );
}
