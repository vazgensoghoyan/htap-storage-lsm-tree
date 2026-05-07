#include "storage/read/sstable/linear_block_selector.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace {

using htap::storage::Key;
using htap::storage::read::sstable::BlockMeta;
using htap::storage::read::sstable::KeyRange;
using htap::storage::read::sstable::LinearBlockSelector;

BlockMeta MakeBlock(Key min_key, Key max_key, std::size_t block_id) {
    return BlockMeta{
        .min_key = min_key,
        .max_key = max_key,
        .offset = static_cast<std::uint64_t>(block_id * 100),
        .size = 100,
        .row_count = 10,
        .block_id = block_id,
    };
}

std::vector<std::size_t> BlockIds(const std::vector<BlockMeta>& blocks) {
    std::vector<std::size_t> ids;

    for (const auto& block : blocks) {
        ids.push_back(block.block_id);
    }

    return ids;
}

TEST(LinearBlockSelectorTest, EmptySelectorSelectsNoBlocks) {
    LinearBlockSelector selector({});

    auto selected = selector.select_blocks(KeyRange{
        .from = 10,
        .to = 20,
    });

    EXPECT_TRUE(selected.empty());
}

TEST(LinearBlockSelectorTest, FullRangeSelectsAllBlocks) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(10, 20, 1),
        MakeBlock(20, 30, 2),
    });

    auto selected = selector.select_blocks(KeyRange{
        .from = std::nullopt,
        .to = std::nullopt,
    });

    EXPECT_EQ(BlockIds(selected), std::vector<std::size_t>({0, 1, 2}));
}

TEST(LinearBlockSelectorTest, SelectsIntersectingBlocksForHalfOpenRange) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(10, 20, 1),
        MakeBlock(20, 30, 2),
        MakeBlock(30, 40, 3),
    });

    auto selected = selector.select_blocks(KeyRange{
        .from = 15,
        .to = 31,
    });

    EXPECT_EQ(BlockIds(selected), std::vector<std::size_t>({1, 2, 3}));
}

TEST(LinearBlockSelectorTest, DoesNotSelectBlockEndingAtRangeFrom) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(10, 20, 1),
        MakeBlock(20, 30, 2),
    });

    auto selected = selector.select_blocks(KeyRange{
        .from = 10,
        .to = 20,
    });

    EXPECT_EQ(BlockIds(selected), std::vector<std::size_t>({1}));
}

TEST(LinearBlockSelectorTest, DoesNotSelectBlockStartingAtExclusiveTo) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(10, 20, 1),
        MakeBlock(20, 30, 2),
    });

    auto selected = selector.select_blocks(KeyRange{
        .from = 0,
        .to = 20,
    });

    EXPECT_EQ(BlockIds(selected), std::vector<std::size_t>({0, 1}));
}

TEST(LinearBlockSelectorTest, SelectsBlockContainingRangeFrom) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(10, 20, 1),
    });

    auto selected = selector.select_blocks(KeyRange{
        .from = 9,
        .to = 11,
    });

    EXPECT_EQ(BlockIds(selected), std::vector<std::size_t>({0, 1}));
}

TEST(LinearBlockSelectorTest, RangeBeforeAllBlocksSelectsNoBlocks) {
    LinearBlockSelector selector({
        MakeBlock(10, 20, 0),
        MakeBlock(20, 30, 1),
    });

    auto selected = selector.select_blocks(KeyRange{
        .from = 0,
        .to = 10,
    });

    EXPECT_TRUE(selected.empty());
}

TEST(LinearBlockSelectorTest, RangeAfterAllBlocksSelectsNoBlocks) {
    LinearBlockSelector selector({
        MakeBlock(10, 20, 0),
        MakeBlock(20, 30, 1),
    });

    auto selected = selector.select_blocks(KeyRange{
        .from = 30,
        .to = 40,
    });

    EXPECT_TRUE(selected.empty());
}

TEST(LinearBlockSelectorTest, OpenEndedFromSelectsPrefixBlocks) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(10, 20, 1),
        MakeBlock(20, 30, 2),
    });

    auto selected = selector.select_blocks(KeyRange{
        .from = std::nullopt,
        .to = 20,
    });

    EXPECT_EQ(BlockIds(selected), std::vector<std::size_t>({0, 1}));
}

TEST(LinearBlockSelectorTest, OpenEndedToSelectsSuffixBlocks) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(10, 20, 1),
        MakeBlock(20, 30, 2),
    });

    auto selected = selector.select_blocks(KeyRange{
        .from = 10,
        .to = std::nullopt,
    });

    EXPECT_EQ(BlockIds(selected), std::vector<std::size_t>({1, 2}));
}

TEST(LinearBlockSelectorTest, SelectBlockForExistingKey) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(10, 20, 1),
        MakeBlock(20, 30, 2),
    });

    auto block = selector.select_block_for_key(15);

    ASSERT_TRUE(block.has_value());
    EXPECT_EQ(block->block_id, 1);
}

TEST(LinearBlockSelectorTest, SelectBlockForKeyOnLowerBoundary) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(10, 20, 1),
    });

    auto block = selector.select_block_for_key(10);

    ASSERT_TRUE(block.has_value());
    EXPECT_EQ(block->block_id, 1);
}

TEST(LinearBlockSelectorTest, SelectBlockForKeyOnExclusiveUpperBoundary) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(10, 20, 1),
    });

    auto block = selector.select_block_for_key(20);

    EXPECT_FALSE(block.has_value());
}

TEST(LinearBlockSelectorTest, SelectBlockForMissingKeyReturnsNullopt) {
    LinearBlockSelector selector({
        MakeBlock(0, 10, 0),
        MakeBlock(20, 30, 1),
    });

    auto block = selector.select_block_for_key(15);

    EXPECT_FALSE(block.has_value());
}

TEST(LinearBlockSelectorTest, ContainsUsesHalfOpenRange) {
    KeyRange range{
        .from = 10,
        .to = 20,
    };

    EXPECT_FALSE(htap::storage::read::sstable::contains(range, 9));
    EXPECT_TRUE(htap::storage::read::sstable::contains(range, 10));
    EXPECT_TRUE(htap::storage::read::sstable::contains(range, 19));
    EXPECT_FALSE(htap::storage::read::sstable::contains(range, 20));
}

}
