#include "storage/read/sstable/linear_column_block_selector.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <utility>
#include <vector>

namespace htap::storage::read::sstable {
namespace {

ColumnBlockMeta make_block(
    Key min_key,
    Key max_key,
    std::size_t block_id,
    std::size_t column_idx
) {
    ColumnBlockMeta block{};
    block.min_key = min_key;
    block.max_key = max_key;
    block.offset = 0;
    block.size = 0;
    block.values_count = 0;
    block.block_id = block_id;
    block.column_idx = column_idx;
    return block;
}

KeyRange make_range(std::optional<Key> from, std::optional<Key> to) {
    KeyRange range;
    range.from = from;
    range.to = to;
    return range;
}

std::vector<ColumnBlockMeta> make_blocks() {
    return {
        make_block(0, 10, 0, 0),
        make_block(0, 10, 0, 1),
        make_block(0, 10, 0, 2),

        make_block(10, 20, 1, 0),
        make_block(10, 20, 1, 1),
        make_block(10, 20, 1, 2),

        make_block(20, 30, 2, 0),
        make_block(20, 30, 2, 1),
        make_block(20, 30, 2, 2),
    };
}

std::vector<std::pair<std::size_t, std::size_t>> block_ids_and_columns(
    const std::vector<ColumnBlockMeta>& blocks
) {
    std::vector<std::pair<std::size_t, std::size_t>> result;
    result.reserve(blocks.size());

    for (const auto& block : blocks) {
        result.emplace_back(block.block_id, block.column_idx);
    }

    return result;
}

void expect_blocks_eq(
    const std::vector<ColumnBlockMeta>& actual_blocks,
    const std::vector<std::pair<std::size_t, std::size_t>>& expected
) {
    EXPECT_TRUE(block_ids_and_columns(actual_blocks) == expected);
}

} 

TEST(LinearColumnBlockSelectorTest, SelectsBlocksIntersectingClosedOpenRange) {
    LinearColumnBlockSelector selector(make_blocks());

    const auto selected = selector.select_blocks(make_range(5, 15));

    expect_blocks_eq(selected, {
        {0, 0}, {0, 1}, {0, 2},
        {1, 0}, {1, 1}, {1, 2},
    });
}

TEST(LinearColumnBlockSelectorTest, RespectsUpperExclusiveBoundary) {
    LinearColumnBlockSelector selector(make_blocks());

    const auto selected = selector.select_blocks(make_range(10, 20));

    expect_blocks_eq(selected, {
        {1, 0}, {1, 1}, {1, 2},
    });
}

TEST(LinearColumnBlockSelectorTest, ReturnsEmptyWhenRangeDoesNotIntersectAnyBlock) {
    LinearColumnBlockSelector selector(make_blocks());

    const auto selected = selector.select_blocks(make_range(30, 40));

    EXPECT_TRUE(selected.empty());
}

TEST(LinearColumnBlockSelectorTest, SupportsOpenLowerBound) {
    LinearColumnBlockSelector selector(make_blocks());

    const auto selected = selector.select_blocks(make_range(std::nullopt, 10));

    expect_blocks_eq(selected, {
        {0, 0}, {0, 1}, {0, 2},
    });
}

TEST(LinearColumnBlockSelectorTest, SupportsOpenUpperBound) {
    LinearColumnBlockSelector selector(make_blocks());

    const auto selected = selector.select_blocks(make_range(20, std::nullopt));

    expect_blocks_eq(selected, {
        {2, 0}, {2, 1}, {2, 2},
    });
}

TEST(LinearColumnBlockSelectorTest, FullRangeSelectsAllBlocks) {
    LinearColumnBlockSelector selector(make_blocks());

    const auto selected = selector.select_blocks(make_range(std::nullopt, std::nullopt));

    expect_blocks_eq(selected, {
        {0, 0}, {0, 1}, {0, 2},
        {1, 0}, {1, 1}, {1, 2},
        {2, 0}, {2, 1}, {2, 2},
    });
}

TEST(LinearColumnBlockSelectorTest, SelectBlockGroupForKeyReturnsAllColumnsOfMatchingLogicalBlock) {
    LinearColumnBlockSelector selector(make_blocks());

    const auto selected = selector.select_block_group_for_key(12);

    expect_blocks_eq(selected, {
        {1, 0}, {1, 1}, {1, 2},
    });
}

TEST(LinearColumnBlockSelectorTest, SelectBlockGroupForKeyRespectsMaxKeyExclusive) {
    LinearColumnBlockSelector selector(make_blocks());

    const auto selected = selector.select_block_group_for_key(20);

    expect_blocks_eq(selected, {
        {2, 0}, {2, 1}, {2, 2},
    });
}

TEST(LinearColumnBlockSelectorTest, SelectBlockGroupForKeyReturnsEmptyWhenKeyIsOutsideAllBlocks) {
    LinearColumnBlockSelector selector(make_blocks());

    const auto selected = selector.select_block_group_for_key(100);

    EXPECT_TRUE(selected.empty());
}

TEST(LinearColumnBlockSelectorTest, PreservesInputOrder) {
    std::vector<ColumnBlockMeta> blocks = {
        make_block(10, 20, 1, 2),
        make_block(10, 20, 1, 0),
        make_block(10, 20, 1, 1),
    };

    LinearColumnBlockSelector selector(blocks);

    const auto selected = selector.select_blocks(make_range(10, 20));

    expect_blocks_eq(selected, {
        {1, 2}, {1, 0}, {1, 1},
    });
}

} 
