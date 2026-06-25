// tests/lsmtree/sstable/metadata/manifest_roundtrip_tests.cpp
//
// Roundtrip tests for SSTableManifest (save → load).
//
// Covers:
//  - Empty registry save/load
//  - Single SST entry (ROW and COLUMN layout)
//  - Multiple levels, multiple SSTs
//  - next_sst_id preservation
//  - Overwrite: second save replaces first
//  - Missing file: load from non-existent path → empty registry, next_sst_id=0
//  - Corrupt magic: load throws runtime_error
//  - Long path string survival
//  - Registry state: level_count, sstable_count, min/max_key after load

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "lsmtree/sstable/metadata/sstable_manifest.hpp"
#include "lsmtree/sstable/metadata/sstable_registry.hpp"
#include "lsmtree/sstable/metadata/sstable_info.hpp"
#include "lsmtree/sstable/format/sst_layout.hpp"

namespace fs = std::filesystem;

using namespace htap::storage;
using namespace htap::lsmtree::sstable;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

SSTableInfo make_sst(uint64_t id,
                     uint32_t level,
                     SSTLayout layout,
                     Key min_key,
                     Key max_key,
                     const std::string& path = "/tmp/sst",
                     uint32_t num_blocks = 1,
                     uint64_t file_size = 4096) {
    return SSTableInfo{
        .id              = id,
        .path            = path,
        .level           = level,
        .min_key         = min_key,
        .max_key         = max_key,
        .file_size_bytes = file_size,
        .num_blocks      = num_blocks,
        .layout          = layout
    };
}

// Fixture
class ManifestRoundtripTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "htap_manifest_test";
        fs::remove_all(test_dir_);
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        fs::remove_all(test_dir_);
    }

    fs::path test_dir_;
};

} // namespace

// ---------------------------------------------------------------------------
// 1. Empty registry
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, EmptyRegistry_SaveLoad_ReturnsEmptyRegistry) {
    SSTableRegistry registry;

    SSTableManifest::save(test_dir_, registry, /*next_sst_id=*/0);

    ASSERT_TRUE(fs::exists(test_dir_ / "manifest.bin"));

    SSTableRegistry loaded;
    uint64_t next_id = 99;
    SSTableManifest::load(test_dir_, loaded, next_id);

    EXPECT_EQ(next_id, 0u);
    EXPECT_EQ(loaded.level_count(), 0u);
}

// ---------------------------------------------------------------------------
// 2. next_sst_id preservation
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, NextSstId_Preserved_AfterSaveLoad) {
    SSTableRegistry registry;
    registry.add(make_sst(0, 0, SSTLayout::ROW, 1, 100, "/tmp/sst0"));

    const uint64_t expected_id = 42;
    SSTableManifest::save(test_dir_, registry, expected_id);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    EXPECT_EQ(next_id, expected_id);
}

TEST_F(ManifestRoundtripTest, NextSstId_LargeValue_Preserved) {
    SSTableRegistry registry;
    const uint64_t large_id = 1'000'000'000ULL;
    SSTableManifest::save(test_dir_, registry, large_id);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    EXPECT_EQ(next_id, large_id);
}

// ---------------------------------------------------------------------------
// 3. Single SST entry — ROW layout
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, SingleEntry_ROW_AllFieldsPreserved) {
    SSTableRegistry registry;
    const auto sst = make_sst(
        /*id=*/7,
        /*level=*/0,
        SSTLayout::ROW,
        /*min_key=*/100,
        /*max_key=*/999,
        /*path=*/"/data/lsm/sst_007",
        /*num_blocks=*/5,
        /*file_size=*/16384u
    );
    registry.add(sst);

    SSTableManifest::save(test_dir_, registry, /*next_sst_id=*/8);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    EXPECT_EQ(next_id, 8u);
    ASSERT_EQ(loaded.level_count(), 1u);
    ASSERT_EQ(loaded.sstable_count(0), 1u);

    const auto& entries = loaded.sstables_at_level(0);
    ASSERT_EQ(entries.size(), 1u);

    const auto& e = entries[0];
    EXPECT_EQ(e.id,              7u);
    EXPECT_EQ(e.level,           0u);
    EXPECT_EQ(e.layout,          SSTLayout::ROW);
    EXPECT_EQ(e.min_key,         100);
    EXPECT_EQ(e.max_key,         999);
    EXPECT_EQ(e.path,            "/data/lsm/sst_007");
    EXPECT_EQ(e.num_blocks,      5u);
    EXPECT_EQ(e.file_size_bytes, 16384u);
}

// ---------------------------------------------------------------------------
// 4. Single SST entry — COLUMN layout
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, SingleEntry_COLUMN_LayoutPreserved) {
    SSTableRegistry registry;
    registry.add(make_sst(3, 1, SSTLayout::COLUMN, 500, 2000, "/var/sst/col_3", 10, 65536u));

    SSTableManifest::save(test_dir_, registry, 4);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    ASSERT_EQ(loaded.level_count(), 2u); // levels 0 and 1
    ASSERT_EQ(loaded.sstable_count(1), 1u);

    const auto& e = loaded.sstables_at_level(1)[0];
    EXPECT_EQ(e.layout,  SSTLayout::COLUMN);
    EXPECT_EQ(e.id,      3u);
    EXPECT_EQ(e.min_key, 500);
    EXPECT_EQ(e.max_key, 2000);
}

// ---------------------------------------------------------------------------
// 5. Multiple levels, multiple SSTs
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, MultipleLevels_MultipleSSTs_AllPreserved) {
    SSTableRegistry registry;

    // L0: 3 ROW sstables
    registry.add(make_sst(1,  0, SSTLayout::ROW,    1,  100, "/tmp/l0_1"));
    registry.add(make_sst(2,  0, SSTLayout::ROW,  101,  200, "/tmp/l0_2"));
    registry.add(make_sst(3,  0, SSTLayout::ROW,  201,  300, "/tmp/l0_3"));

    // L1: 2 COLUMN sstables
    registry.add(make_sst(4,  1, SSTLayout::COLUMN,   1,  150, "/tmp/l1_1"));
    registry.add(make_sst(5,  1, SSTLayout::COLUMN, 151,  300, "/tmp/l1_2"));

    // L2: 1 COLUMN sstable
    registry.add(make_sst(6,  2, SSTLayout::COLUMN,   1,  300, "/tmp/l2_1"));

    SSTableManifest::save(test_dir_, registry, 7);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    EXPECT_EQ(next_id, 7u);
    ASSERT_EQ(loaded.level_count(), 3u);

    EXPECT_EQ(loaded.sstable_count(0), 3u);
    EXPECT_EQ(loaded.sstable_count(1), 2u);
    EXPECT_EQ(loaded.sstable_count(2), 1u);

    // Verify L0 ids
    std::vector<uint64_t> l0_ids;
    for (const auto& e : loaded.sstables_at_level(0))
        l0_ids.push_back(e.id);
    EXPECT_TRUE(std::find(l0_ids.begin(), l0_ids.end(), 1u) != l0_ids.end());
    EXPECT_TRUE(std::find(l0_ids.begin(), l0_ids.end(), 2u) != l0_ids.end());
    EXPECT_TRUE(std::find(l0_ids.begin(), l0_ids.end(), 3u) != l0_ids.end());

    // Verify L1 layouts
    for (const auto& e : loaded.sstables_at_level(1))
        EXPECT_EQ(e.layout, SSTLayout::COLUMN);

    // Verify L2
    const auto& l2 = loaded.sstables_at_level(2);
    ASSERT_EQ(l2.size(), 1u);
    EXPECT_EQ(l2[0].id, 6u);
    EXPECT_EQ(l2[0].min_key, 1);
    EXPECT_EQ(l2[0].max_key, 300);
}

// ---------------------------------------------------------------------------
// 6. Overwrite: second save replaces first
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, SecondSave_OverwritesFirst) {
    SSTableRegistry registry1;
    registry1.add(make_sst(1, 0, SSTLayout::ROW, 1, 100, "/tmp/old_sst"));
    SSTableManifest::save(test_dir_, registry1, 2);

    SSTableRegistry registry2;
    registry2.add(make_sst(10, 0, SSTLayout::COLUMN, 500, 600, "/tmp/new_sst_a"));
    registry2.add(make_sst(11, 0, SSTLayout::COLUMN, 601, 700, "/tmp/new_sst_b"));
    SSTableManifest::save(test_dir_, registry2, 12);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    EXPECT_EQ(next_id, 12u);
    ASSERT_EQ(loaded.sstable_count(0), 2u);

    const auto& entries = loaded.sstables_at_level(0);
    std::vector<uint64_t> ids;
    for (const auto& e : entries) ids.push_back(e.id);

    EXPECT_TRUE(std::find(ids.begin(), ids.end(), 10u) != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), 11u) != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), 1u)  == ids.end()); // old gone
}

// ---------------------------------------------------------------------------
// 7. Missing file: load returns empty state
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, MissingManifest_LoadReturnsEmptyState) {
    // No save — manifest.bin doesn't exist
    SSTableRegistry loaded;
    uint64_t next_id = 99;
    SSTableManifest::load(test_dir_, loaded, next_id);

    EXPECT_EQ(next_id, 0u);
    EXPECT_EQ(loaded.level_count(), 0u);
}

// ---------------------------------------------------------------------------
// 8. Corrupt magic: load throws
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, CorruptMagic_LoadThrows) {
    // Write a manifest with wrong magic
    const auto manifest_path = test_dir_ / "manifest.bin";
    {
        std::ofstream out(manifest_path, std::ios::binary | std::ios::trunc);
        const uint32_t bad_magic = 0xDEADBEEFu;
        out.write(reinterpret_cast<const char*>(&bad_magic), sizeof(bad_magic));
    }

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    EXPECT_THROW(
        SSTableManifest::load(test_dir_, loaded, next_id),
        std::runtime_error
    );
}

// ---------------------------------------------------------------------------
// 9. Long path string survival
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, LongPath_Preserved_UpTo65535Chars) {
    const std::string long_path(2048, 'p'); // 2KB path
    SSTableRegistry registry;
    registry.add(make_sst(1, 0, SSTLayout::ROW, 1, 1, long_path));
    SSTableManifest::save(test_dir_, registry, 2);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    ASSERT_EQ(loaded.sstable_count(0), 1u);
    EXPECT_EQ(loaded.sstables_at_level(0)[0].path, long_path);
}

// ---------------------------------------------------------------------------
// 10. Idempotent: multiple save/load cycles
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, MultipleRoundtrips_StateConsistent) {
    SSTableRegistry registry;
    registry.add(make_sst(1, 0, SSTLayout::ROW, 10, 20, "/tmp/sst_1"));

    for (int cycle = 0; cycle < 5; ++cycle) {
        SSTableManifest::save(test_dir_, registry, static_cast<uint64_t>(cycle + 1));

        SSTableRegistry loaded;
        uint64_t next_id = 0;
        SSTableManifest::load(test_dir_, loaded, next_id);

        ASSERT_EQ(next_id, static_cast<uint64_t>(cycle + 1));
        ASSERT_EQ(loaded.sstable_count(0), 1u);
        EXPECT_EQ(loaded.sstables_at_level(0)[0].id, 1u);
    }
}

// ---------------------------------------------------------------------------
// 11. Mixed ROW + COLUMN at same level preserved
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, SameLevel_MixedLayouts_BothPreserved) {
    // This is unusual but must be stored correctly
    SSTableRegistry registry;
    registry.add(make_sst(1, 0, SSTLayout::ROW,    1, 50,  "/tmp/row_sst"));
    registry.add(make_sst(2, 0, SSTLayout::COLUMN, 51, 100, "/tmp/col_sst"));

    SSTableManifest::save(test_dir_, registry, 3);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    ASSERT_EQ(loaded.sstable_count(0), 2u);

    const auto& entries = loaded.sstables_at_level(0);
    bool has_row    = false;
    bool has_column = false;
    for (const auto& e : entries) {
        if (e.layout == SSTLayout::ROW)    has_row    = true;
        if (e.layout == SSTLayout::COLUMN) has_column = true;
    }
    EXPECT_TRUE(has_row);
    EXPECT_TRUE(has_column);
}

// ---------------------------------------------------------------------------
// 12. SSTableRegistry overlapping() works correctly after load
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, AfterLoad_OverlappingQuery_ReturnsCorrectResults) {
    SSTableRegistry registry;
    // L0: three non-overlapping SSTs
    registry.add(make_sst(1, 0, SSTLayout::ROW,   1,  100, "/tmp/sst_1"));
    registry.add(make_sst(2, 0, SSTLayout::ROW, 101,  200, "/tmp/sst_2"));
    registry.add(make_sst(3, 0, SSTLayout::ROW, 201,  300, "/tmp/sst_3"));

    SSTableManifest::save(test_dir_, registry, 4);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    // Query overlapping [50, 150) → should match SST 1 (1..100) and SST 2 (101..200)
    auto overlapping = loaded.overlapping(0, OptKey{50}, OptKey{150});
    EXPECT_EQ(overlapping.size(), 2u);

    // Query overlapping [300, 400) → should match SST 3 (201..300) since 300 <= max_key
    auto only_last = loaded.overlapping(0, OptKey{300}, OptKey{400});
    EXPECT_EQ(only_last.size(), 1u);
    EXPECT_EQ(only_last[0].id, 3u);

    // Query outside range entirely
    auto none = loaded.overlapping(0, OptKey{1000}, OptKey{2000});
    EXPECT_EQ(none.size(), 0u);
}

// ---------------------------------------------------------------------------
// 13. remove() after load works correctly
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, AfterLoad_Remove_CorrectlyPrunedRegistry) {
    SSTableRegistry registry;
    registry.add(make_sst(10, 0, SSTLayout::ROW, 1, 100, "/tmp/sst_10"));
    registry.add(make_sst(11, 0, SSTLayout::ROW, 101, 200, "/tmp/sst_11"));

    SSTableManifest::save(test_dir_, registry, 12);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    // Remove SST 10
    loaded.remove(10);

    ASSERT_EQ(loaded.sstable_count(0), 1u);
    EXPECT_EQ(loaded.sstables_at_level(0)[0].id, 11u);
}

// ---------------------------------------------------------------------------
// 14. Negative min/max keys preserved correctly
// ---------------------------------------------------------------------------

TEST_F(ManifestRoundtripTest, NegativeKeys_Preserved) {
    SSTableRegistry registry;
    registry.add(make_sst(1, 0, SSTLayout::ROW, -1000, -1, "/tmp/neg_sst"));

    SSTableManifest::save(test_dir_, registry, 2);

    SSTableRegistry loaded;
    uint64_t next_id = 0;
    SSTableManifest::load(test_dir_, loaded, next_id);

    ASSERT_EQ(loaded.sstable_count(0), 1u);
    const auto& e = loaded.sstables_at_level(0)[0];
    EXPECT_EQ(e.min_key, -1000);
    EXPECT_EQ(e.max_key, -1);
}
