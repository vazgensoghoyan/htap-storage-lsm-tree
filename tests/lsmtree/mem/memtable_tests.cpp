#include <gtest/gtest.h>

#include "lsmtree/mem/memtable.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

TEST(MemTableTest, EmptyInitially) {
    MemTable mem;
    EXPECT_EQ(mem.size(), 0);
}

TEST(MemTableTest, InsertIncreasesSize) {
    MemTable mem;

    mem.insert(1, {1, 100});
    mem.insert(2, {2, 200});

    EXPECT_EQ(mem.size(), 2);
}

TEST(MemTableTest, InsertOverwriteSameKey) {
    MemTable mem;

    mem.insert(1, {1, 100});
    mem.insert(1, {1, 999}); // overwrite

    EXPECT_EQ(mem.size(), 1);
}

TEST(MemTableTest, FreezeClearsMemTable) {
    MemTable mem;

    mem.insert(1, {1, 100});
    mem.insert(2, {2, 200});

    auto imm = mem.freeze();

    ASSERT_NE(imm, nullptr);
    EXPECT_EQ(mem.size(), 0);
}

TEST(MemTableTest, FreezeMovesAllData) {
    MemTable mem;

    mem.insert(1, {1, 100});
    mem.insert(2, {2, 200});
    mem.insert(3, {3, 300});

    auto imm = mem.freeze();

    ASSERT_NE(imm, nullptr);
    EXPECT_EQ(imm->size(), 3);
}

TEST(MemTableTest, InsertAfterFreezeWorks) {
    MemTable mem;

    mem.insert(1, {1, 100});
    auto imm = mem.freeze();

    mem.insert(2, {2, 200});

    EXPECT_EQ(mem.size(), 1);
}

TEST(MemTableTest, FreezeEmpty) {
    MemTable mem;

    auto imm = mem.freeze();

    ASSERT_NE(imm, nullptr);
    EXPECT_EQ(imm->size(), 0);
    EXPECT_EQ(mem.size(), 0);
}
