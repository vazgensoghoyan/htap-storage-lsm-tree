#include <gtest/gtest.h>

#include "lsmtree/sstable/metadata/sstable_registry.hpp"

TEST(SSTableRegistryTest, AddCreatesLevels) {
    htap::lsmtree::SSTableRegistry reg;

    htap::lsmtree::SSTableInfo sst1{
        .id = 1,
        .path = "a.sst",
        .level = 0,
        .min_key = 1,
        .max_key = 10,
        .file_size_bytes = 100,
        .num_blocks = 1,
        .layout = htap::lsmtree::SSTLayout::ROW
    };

    htap::lsmtree::SSTableInfo sst2{
        .id = 2,
        .path = "b.sst",
        .level = 2,
        .min_key = 20,
        .max_key = 30,
        .file_size_bytes = 200,
        .num_blocks = 2,
        .layout = htap::lsmtree::SSTLayout::ROW
    };

    reg.add(sst1);
    reg.add(sst2);

    EXPECT_EQ(reg.level_count(), 3);

    EXPECT_EQ(reg.sstable_count(0), 1);
    EXPECT_EQ(reg.sstable_count(1), 0);
    EXPECT_EQ(reg.sstable_count(2), 1);
}

TEST(SSTableRegistryTest, LevelAccessValidAndInvalid) {
    htap::lsmtree::SSTableRegistry reg;

    htap::lsmtree::SSTableInfo sst{
        .id = 1,
        .path = "a.sst",
        .level = 0,
        .min_key = 1,
        .max_key = 10,
        .file_size_bytes = 100,
        .num_blocks = 1,
        .layout = htap::lsmtree::SSTLayout::ROW
    };

    reg.add(sst);

    const auto& lvl0 = reg.sstables_at_level(0);
    EXPECT_EQ(lvl0.size(), 1);

    EXPECT_THROW(reg.sstables_at_level(10), std::out_of_range);
}

TEST(SSTableRegistryTest, OverlappingBasic) {
    htap::lsmtree::SSTableRegistry reg;

    reg.add({
        .id = 1,
        .path = "a.sst",
        .level = 0,
        .min_key = 0,
        .max_key = 10,
        .file_size_bytes = 100,
        .num_blocks = 1,
        .layout = htap::lsmtree::SSTLayout::ROW
    });

    reg.add({
        .id = 2,
        .path = "b.sst",
        .level = 0,
        .min_key = 20,
        .max_key = 30,
        .file_size_bytes = 100,
        .num_blocks = 1,
        .layout = htap::lsmtree::SSTLayout::ROW
    });

    auto res = reg.overlapping(0, 5, 25);

    EXPECT_EQ(res.size(), 2);
}

TEST(SSTableRegistryTest, OverlappingEdgeCases) {
    htap::lsmtree::SSTableRegistry reg;

    reg.add({
        .id = 1,
        .path = "a.sst",
        .level = 0,
        .min_key = 10,
        .max_key = 20,
        .file_size_bytes = 100,
        .num_blocks = 1,
        .layout = htap::lsmtree::SSTLayout::ROW
    });

    auto r1 = reg.overlapping(0, 0, 5);
    EXPECT_TRUE(r1.empty());

    auto r2 = reg.overlapping(0, 21, 30);
    EXPECT_TRUE(r2.empty());

    auto r3 = reg.overlapping(0, std::nullopt, 15);
    EXPECT_EQ(r3.size(), 1);

    auto r4 = reg.overlapping(0, 15, std::nullopt);
    EXPECT_EQ(r4.size(), 1);
}

TEST(SSTableRegistryTest, RemoveById) {
    htap::lsmtree::SSTableRegistry reg;

    reg.add({
        .id = 1,
        .path = "a.sst",
        .level = 0,
        .min_key = 1,
        .max_key = 10,
        .file_size_bytes = 100,
        .num_blocks = 1,
        .layout = htap::lsmtree::SSTLayout::ROW
    });

    reg.add({
        .id = 2,
        .path = "b.sst",
        .level = 0,
        .min_key = 20,
        .max_key = 30,
        .file_size_bytes = 100,
        .num_blocks = 1,
        .layout = htap::lsmtree::SSTLayout::ROW
    });

    EXPECT_EQ(reg.sstable_count(0), 2);

    reg.remove(1);

    EXPECT_EQ(reg.sstable_count(0), 1);

    auto res = reg.overlapping(0, std::nullopt, std::nullopt);
    ASSERT_EQ(res.size(), 1);
    EXPECT_EQ(res[0].id, 2);
}

TEST(SSTableRegistryTest, EmptyRegistry) {
    htap::lsmtree::SSTableRegistry reg;

    EXPECT_EQ(reg.level_count(), 0);
    EXPECT_EQ(reg.sstable_count(0), 0);

    auto res = reg.overlapping(0, 0, 100);
    EXPECT_TRUE(res.empty());
}
