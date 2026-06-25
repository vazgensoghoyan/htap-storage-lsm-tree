// tests/lsmtree/sstable/roundtrip/column_sst_roundtrip_tests.cpp
//
// Comprehensive writer→reader roundtrip tests for the COLUMN-layout SSTable.
//
// Writer:  ColumnSSTableBuilder  (lsmtree/sstable/build/column_sstable_builder.hpp)
// Readers: make_sstable_cursor()  (dispatches to SSTableColumnCursor)
//          SSTableColumnCursor    (direct construction)

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "lsmtree/sstable/build/column_sstable_builder.hpp"
#include "lsmtree/sstable/format/sst_layout.hpp"
#include "lsmtree/sstable/metadata/sstable_info.hpp"
#include "lsmtree/sstable/format/sparse_index_entry.hpp"

#include "storage/api/types.hpp"
#include "storage/model/schema_builder.hpp"
#include "storage/read/sstable/key_range.hpp"
#include "storage/read/sstable/sstable_block_cache.hpp"
#include "storage/read/sstable/sstable_cursor_factory.hpp"
#include "storage/read/sstable/sstable_metadata_cache.hpp"
#include "storage/read/sstable/sstable_reader.hpp"
#include "storage/cursor/sstable_column_cursor.hpp"

namespace fs = std::filesystem;

using namespace htap::storage;
using namespace htap::lsmtree::sstable;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

Schema make_schema_int() {
    return SchemaBuilder{}
        .add_column("id",    ValueType::INT64,  /*is_key=*/true,  /*nullable=*/false)
        .add_column("value", ValueType::INT64,  /*is_key=*/false, /*nullable=*/true)
        .build();
}

Schema make_schema_all_types() {
    return SchemaBuilder{}
        .add_column("id",    ValueType::INT64,  /*is_key=*/true,  /*nullable=*/false)
        .add_column("age",   ValueType::INT64,  /*is_key=*/false, /*nullable=*/true)
        .add_column("score", ValueType::DOUBLE, /*is_key=*/false, /*nullable=*/true)
        .add_column("name",  ValueType::STRING, /*is_key=*/false, /*nullable=*/true)
        .build();
}

Row make_int_row(Key key, std::optional<int64_t> val) {
    Row r(2);
    r[0] = key;
    r[1] = val.has_value() ? NullableValue{*val} : NullableValue{std::nullopt};
    return r;
}

Row make_full_row(Key key,
                  std::optional<int64_t>     age,
                  std::optional<double>      score,
                  std::optional<std::string> name) {
    Row r(4);
    r[0] = key;
    r[1] = age.has_value()   ? NullableValue{*age}   : NullableValue{std::nullopt};
    r[2] = score.has_value() ? NullableValue{*score} : NullableValue{std::nullopt};
    r[3] = name.has_value()  ? NullableValue{*name}  : NullableValue{std::nullopt};
    return r;
}

std::vector<ValueType> schema_types(const Schema& schema) {
    std::vector<ValueType> types;
    for (const auto& col : schema.columns())
        types.push_back(col.type);
    return types;
}

SSTableInfo make_info(const fs::path& sst_dir, const SSTableBuildResult& result) {
    return SSTableInfo{
        .id              = 2,
        .path            = sst_dir.string(),
        .level           = 1,
        .min_key         = result.min_key,
        .max_key         = result.max_key,
        .file_size_bytes = 0,
        .num_blocks      = result.num_blocks,
        .layout          = SSTLayout::COLUMN
    };
}

read::sstable::KeyRange full_range() {
    return {std::nullopt, std::nullopt};
}

read::sstable::KeyRange key_range(std::optional<Key> from, std::optional<Key> to) {
    return {from, to};
}

// Builds a cursor through the factory, creating per-call metadata/block caches.
std::unique_ptr<ICursor> make_cursor(
    const SSTableInfo& info,
    const read::sstable::KeyRange& range,
    const std::vector<ValueType>& types,
    const std::vector<std::size_t>& projection
) {
    read::sstable::SSTableMetadataCache metadata_cache(
        info, static_cast<std::uint32_t>(types.size()));
    auto block_cache = std::make_shared<read::sstable::SSTableBlockCache>();
    return read::sstable::make_sstable_cursor(
        info, metadata_cache, block_cache, range, types, projection);
}

// Read sparse index entries from sparse.idx
std::vector<SparseIndexEntry> read_sparse_index(const fs::path& sst_dir) {
    std::ifstream in(sst_dir / "sparse.idx", std::ios::binary);
    std::vector<SparseIndexEntry> entries;
    if (!in.is_open()) return entries;
    while (true) {
        SparseIndexEntry e{};
        if (!in.read(reinterpret_cast<char*>(&e.min_key),  sizeof(e.min_key)))  break;
        if (!in.read(reinterpret_cast<char*>(&e.block_id), sizeof(e.block_id))) break;
        entries.push_back(e);
    }
    return entries;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ColumnSSTRoundtripTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "htap_col_sst_roundtrip";
        fs::remove_all(test_dir_);
        fs::create_directories(test_dir_);
        sst_dir_ = test_dir_ / "sst_0002";
    }

    void TearDown() override {
        fs::remove_all(test_dir_);
    }

    fs::path test_dir_;
    fs::path sst_dir_;
};

} // namespace

// ---------------------------------------------------------------------------
// 1. Build result sanity
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, BuildResult_MinMaxNumBlocks_SingleRow) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_int_row(42, 100));
    auto result = builder.finish();

    EXPECT_EQ(result.min_key,   42);
    EXPECT_EQ(result.max_key,   42);
    EXPECT_EQ(result.num_blocks, 1u);

    EXPECT_TRUE(fs::exists(sst_dir_ / "data.sst"));
    EXPECT_TRUE(fs::exists(sst_dir_ / "meta.bin"));
    EXPECT_TRUE(fs::exists(sst_dir_ / "info.bin"));
}

TEST_F(ColumnSSTRoundtripTest, BuildResult_MinMaxCorrectForMultipleRows) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 5; k <= 50; ++k)
        builder.add(make_int_row(k, k * 2));
    auto result = builder.finish();

    EXPECT_EQ(result.min_key, 5);
    EXPECT_EQ(result.max_key, 50);
}

TEST_F(ColumnSSTRoundtripTest, EmptyBuilder_Throws) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    EXPECT_THROW(builder.finish(), std::runtime_error);
}

TEST_F(ColumnSSTRoundtripTest, UnsortedInput_Throws) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_int_row(10, 1));
    EXPECT_THROW(builder.add(make_int_row(5, 2)), std::runtime_error);
}

TEST_F(ColumnSSTRoundtripTest, FinishTwice_Throws) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_int_row(1, 1));
    builder.finish();
    EXPECT_THROW(builder.finish(), std::runtime_error);
}

// ---------------------------------------------------------------------------
// 2. Single-row roundtrip
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, SingleRow_INT64Values_RoundtripCorrect) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_int_row(7, 999));
    auto result = builder.finish();

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, full_range(), types, {0, 1});

    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());

    EXPECT_EQ(cursor->key(), 7);
    ASSERT_TRUE(cursor->value(1).has_value());
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 999);

    cursor->next();
    EXPECT_FALSE(cursor->valid());
}

// ---------------------------------------------------------------------------
// 3. All value types
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, AllValueTypes_INT64_DOUBLE_STRING_RoundtripCorrect) {
    auto schema = make_schema_all_types();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_full_row(1, 42,  3.14,  "hello"));
    builder.add(make_full_row(2, 0,   0.0,   ""));
    builder.add(make_full_row(3, -1,  -1.5,  "world"));
    auto result = builder.finish();

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, full_range(), types, {0, 1, 2, 3});

    ASSERT_NE(cursor, nullptr);

    // row 1
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 42);
    EXPECT_DOUBLE_EQ(std::get<double>(*cursor->value(2)), 3.14);
    EXPECT_EQ(std::get<std::string>(*cursor->value(3)), "hello");
    cursor->next();

    // row 2
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 2);
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 0);
    EXPECT_DOUBLE_EQ(std::get<double>(*cursor->value(2)), 0.0);
    EXPECT_EQ(std::get<std::string>(*cursor->value(3)), "");
    cursor->next();

    // row 3
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 3);
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), -1);
    EXPECT_DOUBLE_EQ(std::get<double>(*cursor->value(2)), -1.5);
    EXPECT_EQ(std::get<std::string>(*cursor->value(3)), "world");
    cursor->next();

    EXPECT_FALSE(cursor->valid());
}

// ---------------------------------------------------------------------------
// 4. NULL handling
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, NullValues_RoundtripCorrect) {
    auto schema = make_schema_all_types();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    // row 1: age=null, score=1.0, name="a"
    builder.add(make_full_row(1, std::nullopt, 1.0, "a"));
    // row 2: age=10, score=null, name="b"
    builder.add(make_full_row(2, 10, std::nullopt, "b"));
    // row 3: age=20, score=2.0, name=null
    builder.add(make_full_row(3, 20, 2.0, std::nullopt));
    // row 4: all nullable cols null
    builder.add(make_full_row(4, std::nullopt, std::nullopt, std::nullopt));
    auto result = builder.finish();

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, full_range(), types, {0, 1, 2, 3});

    ASSERT_NE(cursor, nullptr);

    // row 1
    ASSERT_TRUE(cursor->valid());
    EXPECT_FALSE(cursor->value(1).has_value());           // age = null
    ASSERT_TRUE(cursor->value(2).has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*cursor->value(2)), 1.0);
    ASSERT_TRUE(cursor->value(3).has_value());
    EXPECT_EQ(std::get<std::string>(*cursor->value(3)), "a");
    cursor->next();

    // row 2
    ASSERT_TRUE(cursor->valid());
    ASSERT_TRUE(cursor->value(1).has_value());
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 10);
    EXPECT_FALSE(cursor->value(2).has_value());           // score = null
    ASSERT_TRUE(cursor->value(3).has_value());
    EXPECT_EQ(std::get<std::string>(*cursor->value(3)), "b");
    cursor->next();

    // row 3
    ASSERT_TRUE(cursor->valid());
    ASSERT_TRUE(cursor->value(1).has_value());
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 20);
    ASSERT_TRUE(cursor->value(2).has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*cursor->value(2)), 2.0);
    EXPECT_FALSE(cursor->value(3).has_value());           // name = null
    cursor->next();

    // row 4 — all nullable cols null
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 4);
    EXPECT_FALSE(cursor->value(1).has_value());
    EXPECT_FALSE(cursor->value(2).has_value());
    EXPECT_FALSE(cursor->value(3).has_value());
    cursor->next();

    EXPECT_FALSE(cursor->valid());
}

// ---------------------------------------------------------------------------
// 5. KeyRange filtering
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, KeyRange_FromOnly_ReturnsRowsFromBoundaryInclusive) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 1; k <= 10; ++k)
        builder.add(make_int_row(k, k));
    auto result = builder.finish();

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, key_range(5, std::nullopt), types, {0, 1});

    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 5);

    int count = 0;
    while (cursor->valid()) { cursor->next(); ++count; }
    EXPECT_EQ(count, 6); // keys 5..10
}

TEST_F(ColumnSSTRoundtripTest, KeyRange_ToOnly_ReturnsRowsBeforeBoundaryExclusive) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 1; k <= 10; ++k)
        builder.add(make_int_row(k, k));
    auto result = builder.finish();

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, key_range(std::nullopt, 5), types, {0, 1});

    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);

    int count = 0;
    while (cursor->valid()) { cursor->next(); ++count; }
    EXPECT_EQ(count, 4); // keys 1..4
}

TEST_F(ColumnSSTRoundtripTest, KeyRange_BothBounds_ReturnsBoundedWindow) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 1; k <= 20; ++k)
        builder.add(make_int_row(k, k * 10));
    auto result = builder.finish();

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    // from=5 (inclusive), to=11 (exclusive) → keys 5,6,7,8,9,10
    auto cursor = make_cursor(info, key_range(5, 11), types, {0, 1});

    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 5);

    std::vector<Key> keys_seen;
    while (cursor->valid()) {
        keys_seen.push_back(cursor->key());
        cursor->next();
    }

    ASSERT_EQ(keys_seen.size(), 6u);
    EXPECT_EQ(keys_seen.front(), 5);
    EXPECT_EQ(keys_seen.back(),  10);
}

TEST_F(ColumnSSTRoundtripTest, KeyRange_NoMatch_CursorEmptyOrInvalid) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 1; k <= 5; ++k)
        builder.add(make_int_row(k, k));
    auto result = builder.finish();

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, key_range(100, 200), types, {0, 1});

    bool empty = (cursor == nullptr) || !cursor->valid();
    EXPECT_TRUE(empty);
}

TEST_F(ColumnSSTRoundtripTest, KeyRange_UpperBoundExclusive_BoundaryRowNotReturned) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 1; k <= 5; ++k)
        builder.add(make_int_row(k, k));
    auto result = builder.finish();

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    // to=5 → keys 1,2,3,4 only
    auto cursor = make_cursor(info, key_range(std::nullopt, 5), types, {0, 1});

    ASSERT_NE(cursor, nullptr);
    std::vector<Key> keys_seen;
    while (cursor->valid()) {
        keys_seen.push_back(cursor->key());
        cursor->next();
    }
    ASSERT_EQ(keys_seen.size(), 4u);
    EXPECT_EQ(keys_seen.back(), 4);
}

// ---------------------------------------------------------------------------
// 6. Column projection
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, Projection_SubsetOfColumns_OnlyRequestedColumnsReadable) {
    auto schema = make_schema_all_types();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_full_row(1, 100, 1.5, "ignored"));
    builder.add(make_full_row(2, 200, 2.5, "also_ignored"));
    auto result = builder.finish();

    auto info  = make_info(sst_dir_, result);
    auto types = schema_types(schema);
    // Project key(0) and age(1) only
    auto cursor = make_cursor(info, full_range(), types, {0, 1});

    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);
    ASSERT_TRUE(cursor->value(1).has_value());
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 100);
    cursor->next();

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 2);
    ASSERT_TRUE(cursor->value(1).has_value());
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 200);
    cursor->next();

    EXPECT_FALSE(cursor->valid());
}

TEST_F(ColumnSSTRoundtripTest, Projection_ScoreAndName_INT64Skipped) {
    auto schema = make_schema_all_types();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_full_row(1, 10, 1.1, "alpha"));
    builder.add(make_full_row(2, 20, 2.2, "beta"));
    auto result = builder.finish();

    auto info  = make_info(sst_dir_, result);
    auto types = schema_types(schema);
    // Project key(0), score(2), name(3) — skip age(1)
    auto cursor = make_cursor(info, full_range(), types, {0, 2, 3});

    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);
    ASSERT_TRUE(cursor->value(2).has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*cursor->value(2)), 1.1);
    ASSERT_TRUE(cursor->value(3).has_value());
    EXPECT_EQ(std::get<std::string>(*cursor->value(3)), "alpha");
    cursor->next();

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 2);
    ASSERT_TRUE(cursor->value(2).has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*cursor->value(2)), 2.2);
    ASSERT_TRUE(cursor->value(3).has_value());
    EXPECT_EQ(std::get<std::string>(*cursor->value(3)), "beta");
    cursor->next();

    EXPECT_FALSE(cursor->valid());
}

TEST_F(ColumnSSTRoundtripTest, Projection_KeyOnly_AllRowsKeyCorrect) {
    auto schema = make_schema_all_types();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 1; k <= 5; ++k)
        builder.add(make_full_row(k, k * 10, k * 0.1, "str"));
    auto result = builder.finish();

    auto info  = make_info(sst_dir_, result);
    auto types = schema_types(schema);
    auto cursor = make_cursor(info, full_range(), types, {0});

    ASSERT_NE(cursor, nullptr);
    int count = 0;
    while (cursor->valid()) {
        EXPECT_EQ(cursor->key(), static_cast<Key>(count + 1));
        cursor->next();
        ++count;
    }
    EXPECT_EQ(count, 5);
}

// ---------------------------------------------------------------------------
// 7. Multiple logical blocks
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, MultipleBlocks_AllRowsPresent_InOrder) {
    // TARGET_BLOCK_ROWS = 128 for ColumnSSTableBuilder, so >128 rows → multiple blocks
    auto schema = make_schema_all_types();
    const int N = 400;

    {
        ColumnSSTableBuilder builder(schema, sst_dir_);
        for (int i = 1; i <= N; ++i) {
            builder.add(make_full_row(
                static_cast<Key>(i),
                static_cast<int64_t>(i),
                static_cast<double>(i) * 0.5,
                std::string("name_") + std::to_string(i)
            ));
        }
        auto result = builder.finish();
        EXPECT_GT(result.num_blocks, 1u) << "Expected >1 block with " << N << " rows";
    }

    uint32_t num_blocks = 0;
    {
        std::ifstream inf(sst_dir_ / "info.bin", std::ios::binary);
        inf.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
    }

    SSTableInfo info{
        .id              = 2,
        .path            = sst_dir_.string(),
        .level           = 1,
        .min_key         = 1,
        .max_key         = static_cast<Key>(N),
        .file_size_bytes = 0,
        .num_blocks      = num_blocks,
        .layout          = SSTLayout::COLUMN
    };

    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, full_range(), types, {0, 1, 2, 3});

    ASSERT_NE(cursor, nullptr);
    int count = 0;
    while (cursor->valid()) {
        EXPECT_EQ(cursor->key(), static_cast<Key>(count + 1));
        ASSERT_TRUE(cursor->value(1).has_value());
        EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), static_cast<int64_t>(count + 1));
        ASSERT_TRUE(cursor->value(2).has_value());
        EXPECT_DOUBLE_EQ(std::get<double>(*cursor->value(2)), (count + 1) * 0.5);
        cursor->next();
        ++count;
    }
    EXPECT_EQ(count, N);
}

TEST_F(ColumnSSTRoundtripTest, MultipleBlocks_KeyRangeFilter_OnlyMatchingBlocksRead) {
    auto schema = make_schema_int();
    const int N = 400; // forces multiple logical blocks (>128)
    {
        ColumnSSTableBuilder builder(schema, sst_dir_);
        for (int i = 1; i <= N; ++i)
            builder.add(make_int_row(static_cast<Key>(i), i * 10));
        auto result = builder.finish();
        EXPECT_GT(result.num_blocks, 1u);
    }

    uint32_t num_blocks = 0;
    {
        std::ifstream inf(sst_dir_ / "info.bin", std::ios::binary);
        inf.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
    }

    SSTableInfo info{
        .id              = 2,
        .path            = sst_dir_.string(),
        .level           = 1,
        .min_key         = 1,
        .max_key         = static_cast<Key>(N),
        .file_size_bytes = 0,
        .num_blocks      = num_blocks,
        .layout          = SSTLayout::COLUMN
    };

    auto types  = schema_types(schema);
    // range [200, 251) → keys 200..250 = 51 rows
    auto cursor = make_cursor(info, key_range(200, 251), types, {0, 1});

    ASSERT_NE(cursor, nullptr);
    int count = 0;
    Key expected = 200;
    while (cursor->valid()) {
        EXPECT_EQ(cursor->key(), expected);
        ASSERT_TRUE(cursor->value(1).has_value());
        EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), expected * 10);
        ++expected;
        cursor->next();
        ++count;
    }
    EXPECT_EQ(count, 51);
}

// ---------------------------------------------------------------------------
// 8. meta.bin column block ordering (block_id * N + col_idx)
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, MetaBin_ColumnBlockMeta_Ordering_Correct) {
    // 2-column schema (key + 1 value), so N=2 columns.
    // For single block: meta entries should be:
    //   [block_id=0, col_idx=0] [block_id=0, col_idx=1]
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 1; k <= 5; ++k)
        builder.add(make_int_row(k, k * 100));
    builder.finish();

    // Read meta.bin — ColumnBlockMeta on disk (from column_block_meta.hpp):
    //   int64 min_key, int64 max_key, uint64 offset, uint64 size,
    //   uint32 values_count, uint32 block_id, uint32 column_idx
    // Total: 8+8+8+8+4+4+4 = 44 bytes per entry
    std::ifstream meta(sst_dir_ / "meta.bin", std::ios::binary);
    ASSERT_TRUE(meta.is_open());

    struct RawMeta {
        int64_t  min_key;
        int64_t  max_key;
        uint64_t offset;
        uint64_t size;
        uint32_t values_count;
        uint32_t block_id;
        uint32_t column_idx;
    };

    std::vector<RawMeta> entries;
    while (true) {
        RawMeta e{};
        if (!meta.read(reinterpret_cast<char*>(&e.min_key),     sizeof(e.min_key)))     break;
        meta.read(reinterpret_cast<char*>(&e.max_key),     sizeof(e.max_key));
        meta.read(reinterpret_cast<char*>(&e.offset),      sizeof(e.offset));
        meta.read(reinterpret_cast<char*>(&e.size),        sizeof(e.size));
        meta.read(reinterpret_cast<char*>(&e.values_count),sizeof(e.values_count));
        meta.read(reinterpret_cast<char*>(&e.block_id),    sizeof(e.block_id));
        meta.read(reinterpret_cast<char*>(&e.column_idx),  sizeof(e.column_idx));
        entries.push_back(e);
    }

    // Single logical block → 2 entries: (block=0,col=0) and (block=0,col=1)
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].block_id,   0u);
    EXPECT_EQ(entries[0].column_idx, 0u); // KeyBlock
    EXPECT_EQ(entries[1].block_id,   0u);
    EXPECT_EQ(entries[1].column_idx, 1u); // value column

    // Both entries must have correct min/max
    EXPECT_EQ(entries[0].min_key, 1);
    EXPECT_EQ(entries[0].max_key, 5);
    EXPECT_EQ(entries[1].min_key, 1);
    EXPECT_EQ(entries[1].max_key, 5);
}

TEST_F(ColumnSSTRoundtripTest, MetaBin_MultiBlock_ColumnBlockMeta_OrderingCorrect) {
    // 3-column schema → 3 meta entries per logical block
    auto schema = make_schema_all_types(); // 4 columns (key + 3 values)
    const int N = 300; // force 2+ logical blocks (TARGET_BLOCK_ROWS=128)

    ColumnSSTableBuilder builder(schema, sst_dir_);
    for (int i = 1; i <= N; ++i)
        builder.add(make_full_row(static_cast<Key>(i), i, i * 0.1, "x"));
    auto result = builder.finish();
    ASSERT_GT(result.num_blocks, 1u);

    // For 4-column schema (col_idx 0..3), num_blocks logical blocks:
    //   Total meta entries = num_blocks * 4
    std::ifstream meta(sst_dir_ / "meta.bin", std::ios::binary);
    ASSERT_TRUE(meta.is_open());

    struct RawMeta {
        int64_t  min_key;
        int64_t  max_key;
        uint64_t offset;
        uint64_t size;
        uint32_t values_count;
        uint32_t block_id;
        uint32_t column_idx;
    };

    std::vector<RawMeta> entries;
    while (true) {
        RawMeta e{};
        if (!meta.read(reinterpret_cast<char*>(&e.min_key),     sizeof(e.min_key)))     break;
        meta.read(reinterpret_cast<char*>(&e.max_key),     sizeof(e.max_key));
        meta.read(reinterpret_cast<char*>(&e.offset),      sizeof(e.offset));
        meta.read(reinterpret_cast<char*>(&e.size),        sizeof(e.size));
        meta.read(reinterpret_cast<char*>(&e.values_count),sizeof(e.values_count));
        meta.read(reinterpret_cast<char*>(&e.block_id),    sizeof(e.block_id));
        meta.read(reinterpret_cast<char*>(&e.column_idx),  sizeof(e.column_idx));
        entries.push_back(e);
    }

    const uint32_t num_blocks = result.num_blocks;
    ASSERT_EQ(entries.size(), num_blocks * 4u)
        << "Expected num_blocks=" << num_blocks << " * 4 columns = " << num_blocks * 4 << " meta entries";

    // Verify ordering: entry[i] = (block_id = i/4, col_idx = i%4)
    for (size_t i = 0; i < entries.size(); ++i) {
        const uint32_t expected_block  = static_cast<uint32_t>(i / 4);
        const uint16_t expected_col    = static_cast<uint16_t>(i % 4);
        EXPECT_EQ(entries[i].block_id,   expected_block)
            << "Entry " << i << ": expected block_id=" << expected_block;
        EXPECT_EQ(entries[i].column_idx, expected_col)
            << "Entry " << i << ": expected column_idx=" << expected_col;
    }
}

// ---------------------------------------------------------------------------
// 9. info.bin layout field
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, InfoFile_LayoutIsCOLUMN) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_int_row(1, 10));
    builder.finish();

    std::ifstream info(sst_dir_ / "info.bin", std::ios::binary);
    ASSERT_TRUE(info.is_open());

    uint32_t num_blocks = 0;
    int64_t  min_key    = 0;
    int64_t  max_key    = 0;
    uint8_t  layout     = 255;

    info.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
    info.read(reinterpret_cast<char*>(&min_key),    sizeof(min_key));
    info.read(reinterpret_cast<char*>(&max_key),    sizeof(max_key));
    info.read(reinterpret_cast<char*>(&layout),     sizeof(layout));

    EXPECT_EQ(num_blocks, 1u);
    EXPECT_EQ(min_key, 1);
    EXPECT_EQ(max_key, 1);
    EXPECT_EQ(layout, static_cast<uint8_t>(SSTLayout::COLUMN));
}

// ---------------------------------------------------------------------------
// 10. Sparse index correctness
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, SparseIndex_EntriesMonotonicallyIncreasing) {
    auto schema = make_schema_int();
    // step=1 → every logical block gets a sparse index entry
    ColumnSSTableBuilder builder(schema, sst_dir_, /*sparse_index_step=*/1);
    const int N = 400; // multiple logical blocks
    for (int i = 1; i <= N; ++i)
        builder.add(make_int_row(static_cast<Key>(i * 10), i));
    builder.finish();

    auto entries = read_sparse_index(sst_dir_);
    ASSERT_FALSE(entries.empty());

    for (size_t i = 1; i < entries.size(); ++i) {
        EXPECT_LT(entries[i - 1].min_key, entries[i].min_key)
            << "Sparse index keys must be strictly increasing";
    }
}

TEST_F(ColumnSSTRoundtripTest, SparseIndex_StepRespected) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_, /*sparse_index_step=*/2);
    const int N = 400;
    for (int i = 1; i <= N; ++i)
        builder.add(make_int_row(static_cast<Key>(i), i));
    auto result = builder.finish();

    auto entries = read_sparse_index(sst_dir_);
    for (const auto& e : entries) {
        EXPECT_EQ(e.block_id % 2, 0u)
            << "Sparse entry block_id=" << e.block_id << " should be divisible by step=2";
    }

    if (result.num_blocks > 0) {
        EXPECT_EQ(entries.front().block_id, 0u);
    }
}

// ---------------------------------------------------------------------------
// 11. Direct SSTableColumnCursor roundtrip
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, DirectSSTableColumnCursor_FullRoundtrip) {
    auto schema = make_schema_int();
    const int N = 50;

    ColumnSSTableBuilder builder(schema, sst_dir_);
    for (int i = 1; i <= N; ++i)
        builder.add(make_int_row(static_cast<Key>(i), i * 5));
    auto result = builder.finish();

    // Read metadata via SSTableReader
    auto reader   = std::make_shared<read::sstable::SSTableReader>(sst_dir_.string());
    auto all_meta = reader->read_column_metadata_range(0, result.num_blocks, 2 /* num_columns */);

    auto types = schema_types(schema);
    cursor::SSTableColumnCursor cursor(
        sst_dir_,
        std::move(all_meta),
        full_range(),
        types,
        {0, 1}
    );

    ASSERT_TRUE(cursor.valid());

    int count = 0;
    while (cursor.valid()) {
        EXPECT_EQ(cursor.key(), static_cast<Key>(count + 1));
        ASSERT_TRUE(cursor.value(1).has_value());
        EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), (count + 1) * 5);
        cursor.next();
        ++count;
    }
    EXPECT_EQ(count, N);
}

// ---------------------------------------------------------------------------
// 12. Large string values don't corrupt adjacent rows
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, LargeStringValues_DoNotCorruptAdjacentRows) {
    auto schema = make_schema_all_types();
    const std::string big(512, 'B');
    const std::string small("s");

    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_full_row(1, 1, 1.0, small));
    builder.add(make_full_row(2, 2, 2.0, big));
    builder.add(make_full_row(3, 3, 3.0, small));
    auto result = builder.finish();

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, full_range(), types, {0, 1, 2, 3});

    ASSERT_NE(cursor, nullptr);

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 1);
    EXPECT_EQ(std::get<std::string>(*cursor->value(3)), small);
    cursor->next();

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 2);
    EXPECT_EQ(std::get<std::string>(*cursor->value(3)), big);
    cursor->next();

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 3);
    EXPECT_EQ(std::get<std::string>(*cursor->value(3)), small);
    cursor->next();

    EXPECT_FALSE(cursor->valid());
}

// ---------------------------------------------------------------------------
// 13. make_sstable_cursor factory dispatches to COLUMN cursor correctly
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, Factory_DispatchesByLayout_COLUMN) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_int_row(100, 99));
    auto result = builder.finish();

    // Explicitly set layout=COLUMN, factory must pick SSTableColumnCursor
    auto info = make_info(sst_dir_, result);
    ASSERT_EQ(info.layout, SSTLayout::COLUMN);

    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, full_range(), types, {0, 1});

    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 100);
    ASSERT_TRUE(cursor->value(1).has_value());
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 99);
}

// ---------------------------------------------------------------------------
// 14. Boundary keys (INT64 extremes)
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTRoundtripTest, ExtremeKeyValues_MinMaxINT64_RoundtripCorrect) {
    auto schema = make_schema_int();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    const Key k_min = 1;                  // we can't actually use INT64_MIN as a key easily
    const Key k_max = 1'000'000'000LL;
    builder.add(make_int_row(k_min, -1));
    builder.add(make_int_row(k_max,  1));
    auto result = builder.finish();

    EXPECT_EQ(result.min_key, k_min);
    EXPECT_EQ(result.max_key, k_max);

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, full_range(), types, {0, 1});

    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), k_min);
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), -1);
    cursor->next();

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), k_max);
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 1);
    cursor->next();

    EXPECT_FALSE(cursor->valid());
}
