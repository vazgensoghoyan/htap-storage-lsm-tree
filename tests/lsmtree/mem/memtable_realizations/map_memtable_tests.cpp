#include <gtest/gtest.h>

#include "lsmtree/mem/memtable_realizations/map_memtable.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

TEST(MapMemTableTest, EmptyInitially) {
    MapMemTable mem;
    EXPECT_EQ(mem.size(), 0);
}

TEST(MapMemTableTest, InsertIncreasesSize) {
    MapMemTable mem;

    mem.insert({1, 100});
    mem.insert({2, 200});

    EXPECT_EQ(mem.size(), 2);
}

TEST(MapMemTableTest, InsertOverwriteSameKey) {
    MapMemTable mem;

    mem.insert({1, 100});
    mem.insert({1, 999}); // overwrite

    EXPECT_EQ(mem.size(), 1);
}

TEST(MapMemTableTest, FreezeClearsMapMemTable) {
    MapMemTable mem;

    mem.insert({1, 100});
    mem.insert({2, 200});

    auto imm = mem.to_sorted_immutable();

    ASSERT_NE(imm, nullptr);
    EXPECT_EQ(mem.size(), 0);
}

TEST(MapMemTableTest, FreezeMovesAllData) {
    MapMemTable mem;

    mem.insert({1, 100});
    mem.insert({2, 200});
    mem.insert({3, 300});

    auto imm = mem.to_sorted_immutable();

    ASSERT_NE(imm, nullptr);
    EXPECT_EQ(imm->size(), 3);
}

TEST(MapMemTableTest, InsertAfterFreezeWorks) {
    MapMemTable mem;

    mem.insert({1, 100});
    auto imm = mem.to_sorted_immutable();

    mem.insert({2, 200});

    EXPECT_EQ(mem.size(), 1);
}

TEST(MapMemTableTest, FreezeEmpty) {
    MapMemTable mem;

    auto imm = mem.to_sorted_immutable();

    ASSERT_NE(imm, nullptr);
    EXPECT_EQ(imm->size(), 0);
    EXPECT_EQ(mem.size(), 0);
}
