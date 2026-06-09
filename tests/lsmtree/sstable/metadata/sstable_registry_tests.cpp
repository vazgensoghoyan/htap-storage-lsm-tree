#include <gtest/gtest.h>

#include "lsmtree/sstable/metadata/sstable_registry.hpp"
#include "lsmtree/sstable/metadata/sstable_info.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

namespace {

// ===== Test factory =====

SSTableInfo make_sstable_info(
    uint64_t id,
    std::string path,
    uint32_t level,
    Key min_key,
    Key max_key,
    uint64_t file_size_bytes = 100,
    uint32_t num_blocks = 1,
    SSTLayout layout = SSTLayout::ROW,
    uint64_t meta_offset = 0
) {
    return SSTableInfo{
        .id = id,
        .path = std::move(path),
        .level = level,
        .min_key = min_key,
        .max_key = max_key,
        .file_size_bytes = file_size_bytes,
        .meta_offset = meta_offset,
        .num_blocks = num_blocks,
        .layout = layout
    };
}

} // namespace


// ===================== TESTS =====================

TEST(SSTableRegistryTest, AddCreatesLevels) {
    SSTableRegistry reg;

    auto sst1 = make_sstable_info(1, "a.sst", 0, 1, 10);
    auto sst2 = make_sstable_info(2, "b.sst", 2, 20, 30, 200, 2);

    reg.add(sst1);
    reg.add(sst2);

    EXPECT_EQ(reg.level_count(), 3);

    EXPECT_EQ(reg.sstable_count(0), 1);
    EXPECT_EQ(reg.sstable_count(1), 0);
    EXPECT_EQ(reg.sstable_count(2), 1);
}


TEST(SSTableRegistryTest, LevelAccessValidAndInvalid) {
    SSTableRegistry reg;

    reg.add(make_sstable_info(1, "a.sst", 0, 1, 10));

    const auto& lvl0 = reg.sstables_at_level(0);
    EXPECT_EQ(lvl0.size(), 1);

    EXPECT_THROW(reg.sstables_at_level(10), std::out_of_range);
}


TEST(SSTableRegistryTest, OverlappingBasic) {
    SSTableRegistry reg;

    reg.add(make_sstable_info(1, "a.sst", 0, 0, 10));
    reg.add(make_sstable_info(2, "b.sst", 0, 20, 30));

    auto res = reg.overlapping(0, 5, 25);

    EXPECT_EQ(res.size(), 2);
}


TEST(SSTableRegistryTest, OverlappingEdgeCases) {
    SSTableRegistry reg;

    reg.add(make_sstable_info(1, "a.sst", 0, 10, 20));

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
    SSTableRegistry reg;

    reg.add(make_sstable_info(1, "a.sst", 0, 1, 10));
    reg.add(make_sstable_info(2, "b.sst", 0, 20, 30));

    EXPECT_EQ(reg.sstable_count(0), 2);

    reg.remove(1);

    EXPECT_EQ(reg.sstable_count(0), 1);

    auto res = reg.overlapping(0, std::nullopt, std::nullopt);
    ASSERT_EQ(res.size(), 1);
    EXPECT_EQ(res[0].id, 2);
}


TEST(SSTableRegistryTest, EmptyRegistry) {
    SSTableRegistry reg;

    EXPECT_EQ(reg.level_count(), 0);
    EXPECT_EQ(reg.sstable_count(0), 0);

    auto res = reg.overlapping(0, 0, 100);
    EXPECT_TRUE(res.empty());
}
