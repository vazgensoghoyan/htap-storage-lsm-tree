// tests/lsmtree/sstable/roundtrip/row_sst_roundtrip_tests.cpp
//
// Comprehensive writer→reader roundtrip tests for the ROW-layout SSTable.
//
// Writer:  RowSSTableBuilder  (lsmtree/sstable/build/row_sstable_builder.hpp)
// Readers: make_sstable_cursor()  (dispatches to SSTableRowCursor)
//          SSTableRowCursor       (direct construction)

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "lsmtree/sstable/build/row_sstable_builder.hpp"
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
#include "storage/cursor/sstable_row_cursor.hpp"

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
                  std::optional<int64_t> age,
                  std::optional<double>  score,
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
        .id              = 1,
        .path            = sst_dir.string(),
        .level           = 0,
        .min_key         = result.min_key,
        .max_key         = result.max_key,
        .file_size_bytes = 0,
        .num_blocks      = result.num_blocks,
        .layout          = SSTLayout::ROW
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

// Collect all rows from a cursor into a vector of {key, row}
struct RowResult {
    Key key;
    Row values; // as returned by cursor->value(i) per column index
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class RowSSTRoundtripTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "htap_row_sst_roundtrip";
        fs::remove_all(test_dir_);
        fs::create_directories(test_dir_);
        sst_dir_ = test_dir_ / "sst_0001";
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

TEST_F(RowSSTRoundtripTest, BuildResult_MinMaxNumBlocks_SingleRow) {
    auto schema = make_schema_int();
    RowSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_int_row(42, 100));
    auto result = builder.finish();

    EXPECT_EQ(result.min_key,   42);
    EXPECT_EQ(result.max_key,   42);
    EXPECT_EQ(result.num_blocks, 1u);

    EXPECT_TRUE(fs::exists(sst_dir_ / "data.sst"));
    EXPECT_TRUE(fs::exists(sst_dir_ / "meta.bin"));
    EXPECT_TRUE(fs::exists(sst_dir_ / "sparse.idx"));
    EXPECT_TRUE(fs::exists(sst_dir_ / "info.bin"));
}

TEST_F(RowSSTRoundtripTest, BuildResult_MinMaxCorrectForMultipleRows) {
    auto schema = make_schema_int();
    RowSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 5; k <= 50; ++k)
        builder.add(make_int_row(k, k * 2));
    auto result = builder.finish();

    EXPECT_EQ(result.min_key, 5);
    EXPECT_EQ(result.max_key, 50);
}

TEST_F(RowSSTRoundtripTest, EmptyBuilder_Throws) {
    auto schema = make_schema_int();
    RowSSTableBuilder builder(schema, sst_dir_);
    EXPECT_THROW(builder.finish(), std::runtime_error);
}

TEST_F(RowSSTRoundtripTest, UnsortedInput_Throws) {
    auto schema = make_schema_int();
    RowSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_int_row(10, 1));
    EXPECT_THROW(builder.add(make_int_row(5, 2)), std::runtime_error);
}

// ---------------------------------------------------------------------------
// 2. Single-row roundtrip
// ---------------------------------------------------------------------------

TEST_F(RowSSTRoundtripTest, SingleRow_INT64Values_RoundtripCorrect) {
    auto schema = make_schema_int();

    RowSSTableBuilder builder(schema, sst_dir_);
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

TEST_F(RowSSTRoundtripTest, AllValueTypes_INT64_DOUBLE_STRING_RoundtripCorrect) {
    auto schema = make_schema_all_types();

    RowSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_full_row(1, 42, 3.14, "hello"));
    builder.add(make_full_row(2, 0,  0.0,  ""));
    builder.add(make_full_row(3, -1, -1.5, "world"));
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

TEST_F(RowSSTRoundtripTest, NullValues_RoundtripCorrect) {
    auto schema = make_schema_all_types();

    RowSSTableBuilder builder(schema, sst_dir_);
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

TEST_F(RowSSTRoundtripTest, KeyRange_FromOnly_ReturnsRowsFromBoundaryInclusive) {
    auto schema = make_schema_int();
    RowSSTableBuilder builder(schema, sst_dir_);
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

TEST_F(RowSSTRoundtripTest, KeyRange_ToOnly_ReturnsRowsBeforeBoundaryExclusive) {
    auto schema = make_schema_int();
    RowSSTableBuilder builder(schema, sst_dir_);
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

TEST_F(RowSSTRoundtripTest, KeyRange_BothBounds_ReturnsBoundedWindow) {
    auto schema = make_schema_int();
    RowSSTableBuilder builder(schema, sst_dir_);
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

TEST_F(RowSSTRoundtripTest, KeyRange_NoMatch_CursorInvalid) {
    auto schema = make_schema_int();
    RowSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 1; k <= 5; ++k)
        builder.add(make_int_row(k, k));
    auto result = builder.finish();

    auto info   = make_info(sst_dir_, result);
    auto types  = schema_types(schema);
    // range 100..200 — no rows exist there
    auto cursor = make_cursor(info, key_range(100, 200), types, {0, 1});

    // cursor may be null or invalid
    bool empty = (cursor == nullptr) || !cursor->valid();
    EXPECT_TRUE(empty);
}

TEST_F(RowSSTRoundtripTest, KeyRange_UpperBoundExclusive_BoundaryRowNotReturned) {
    auto schema = make_schema_int();
    RowSSTableBuilder builder(schema, sst_dir_);
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

TEST_F(RowSSTRoundtripTest, Projection_SubsetOfColumns_OnlyRequestedColumnsReadable) {
    auto schema = make_schema_all_types();
    RowSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_full_row(1, 100, 1.5, "ignored"));
    builder.add(make_full_row(2, 200, 2.5, "also_ignored"));
    auto result = builder.finish();

    auto info  = make_info(sst_dir_, result);
    auto types = schema_types(schema);
    // Only project key(0) and age(1)
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

TEST_F(RowSSTRoundtripTest, Projection_KeyOnly_NothingElseAccessed) {
    auto schema = make_schema_all_types();
    RowSSTableBuilder builder(schema, sst_dir_);
    for (Key k = 1; k <= 3; ++k)
        builder.add(make_full_row(k, k * 10, k * 0.1, "str"));
    auto result = builder.finish();

    auto info  = make_info(sst_dir_, result);
    auto types = schema_types(schema);
    // Only project key(0)
    auto cursor = make_cursor(info, full_range(), types, {0});

    ASSERT_NE(cursor, nullptr);
    int count = 0;
    while (cursor->valid()) {
        EXPECT_EQ(cursor->key(), static_cast<Key>(count + 1));
        cursor->next();
        ++count;
    }
    EXPECT_EQ(count, 3);
}

// ---------------------------------------------------------------------------
// 7. Multiple blocks
// ---------------------------------------------------------------------------

TEST_F(RowSSTRoundtripTest, MultipleBlocks_AllRowsPresent_InOrder) {
    // Force multiple blocks by using a small sparse_index_step
    // (blocks split by TARGET_BLOCK_SIZE_BYTES = 4KB)
    // Write strings to fill blocks faster
    auto schema = make_schema_all_types();

    const int N = 500;
    {
        RowSSTableBuilder builder(schema, sst_dir_);
        for (int i = 1; i <= N; ++i) {
            builder.add(make_full_row(
                static_cast<Key>(i),
                static_cast<int64_t>(i),
                static_cast<double>(i) * 0.5,
                std::string(50, 'x') // big enough to fill blocks
            ));
        }
        auto result = builder.finish();
        EXPECT_GT(result.num_blocks, 1u) << "Expected multiple blocks with 500 rows of 50-char strings";
    }

    // Read info.bin to get actual num_blocks
    uint32_t num_blocks = 0;
    {
        std::ifstream inf(sst_dir_ / "info.bin", std::ios::binary);
        inf.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
    }

    SSTableInfo info{
        .id              = 1,
        .path            = sst_dir_.string(),
        .level           = 0,
        .min_key         = 1,
        .max_key         = static_cast<Key>(N),
        .file_size_bytes = 0,
        .num_blocks      = num_blocks,
        .layout          = SSTLayout::ROW
    };

    auto types  = schema_types(schema);
    auto cursor = make_cursor(info, full_range(), types, {0, 1, 2, 3});

    ASSERT_NE(cursor, nullptr);
    int count = 0;
    while (cursor->valid()) {
        EXPECT_EQ(cursor->key(), static_cast<Key>(count + 1));
        ASSERT_TRUE(cursor->value(1).has_value());
        EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), static_cast<int64_t>(count + 1));
        cursor->next();
        ++count;
    }
    EXPECT_EQ(count, N);
}

TEST_F(RowSSTRoundtripTest, MultipleBlocks_KeyRangeFilter_OnlyMatchingRowsReturned) {
    auto schema = make_schema_int();
    const int N = 500;
    {
        RowSSTableBuilder builder(schema, sst_dir_);
        for (int i = 1; i <= N; ++i)
            builder.add(make_int_row(static_cast<Key>(i * 2), i * 2)); // even keys: 2,4,...,1000
        auto result = builder.finish();
        EXPECT_GT(result.num_blocks, 1u);
    }

    uint32_t num_blocks = 0;
    {
        std::ifstream inf(sst_dir_ / "info.bin", std::ios::binary);
        inf.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
    }

    SSTableInfo info{
        .id              = 1,
        .path            = sst_dir_.string(),
        .level           = 0,
        .min_key         = 2,
        .max_key         = static_cast<Key>(N * 2),
        .file_size_bytes = 0,
        .num_blocks      = num_blocks,
        .layout          = SSTLayout::ROW
    };

    auto types  = schema_types(schema);
    // filter keys 100..201 (exclusive) → even keys 100,102,...,200 = 51 rows
    auto cursor = make_cursor(info, key_range(100, 201), types, {0, 1});

    ASSERT_NE(cursor, nullptr);
    int count = 0;
    Key expected = 100;
    while (cursor->valid()) {
        EXPECT_EQ(cursor->key(), expected);
        expected += 2;
        cursor->next();
        ++count;
    }
    EXPECT_EQ(count, 51); // 100,102,...,200
}

// ---------------------------------------------------------------------------
// 8. Sparse index correctness
// ---------------------------------------------------------------------------

TEST_F(RowSSTRoundtripTest, SparseIndex_EntriesMonotonicallyIncreasing) {
    auto schema = make_schema_int();
    // Use sparse_index_step=1 so every block gets an entry
    RowSSTableBuilder builder(schema, sst_dir_, /*sparse_index_step=*/1);
    for (int i = 1; i <= 200; ++i)
        builder.add(make_int_row(static_cast<Key>(i * 10), i));
    builder.finish();

    auto entries = read_sparse_index(sst_dir_);
    ASSERT_FALSE(entries.empty());

    for (size_t i = 1; i < entries.size(); ++i) {
        EXPECT_LT(entries[i - 1].min_key, entries[i].min_key)
            << "Sparse index min_keys must be strictly increasing";
    }
}

TEST_F(RowSSTRoundtripTest, SparseIndex_StepRespected) {
    auto schema = make_schema_int();
    // With step=3: every 3rd block gets an entry
    RowSSTableBuilder builder(schema, sst_dir_, /*sparse_index_step=*/3);
    for (int i = 1; i <= 200; ++i)
        builder.add(make_int_row(static_cast<Key>(i * 10), i));
    auto result = builder.finish();

    auto entries = read_sparse_index(sst_dir_);
    for (const auto& e : entries) {
        EXPECT_EQ(e.block_id % 3, 0u)
            << "Sparse entry block_id=" << e.block_id << " should be divisible by step=3";
    }

    if (result.num_blocks > 0) {
        EXPECT_EQ(entries.front().block_id, 0u);
    }
}

// ---------------------------------------------------------------------------
// 9. info.bin layout field
// ---------------------------------------------------------------------------

TEST_F(RowSSTRoundtripTest, InfoFile_LayoutIsROW) {
    auto schema = make_schema_int();
    RowSSTableBuilder builder(schema, sst_dir_);
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
    EXPECT_EQ(layout, static_cast<uint8_t>(SSTLayout::ROW));
}

// ---------------------------------------------------------------------------
// 10. meta.bin block offsets monotonic
// ---------------------------------------------------------------------------

TEST_F(RowSSTRoundtripTest, MetaBin_BlockOffsetsMonotonicallyIncreasing) {
    auto schema = make_schema_all_types();
    // Force many blocks via string data
    RowSSTableBuilder builder(schema, sst_dir_);
    for (int i = 1; i <= 300; ++i)
        builder.add(make_full_row(static_cast<Key>(i), i, i * 0.1, std::string(30, 'z')));
    auto result = builder.finish();
    ASSERT_GT(result.num_blocks, 1u);

    // RowBlockMeta on-disk layout (from row_block_meta.hpp):
    //   int64 min_key, int64 max_key, uint64 offset, uint64 size, uint32 row_count, uint32 block_id
    std::ifstream meta(sst_dir_ / "meta.bin", std::ios::binary);
    ASSERT_TRUE(meta.is_open());

    std::vector<uint64_t> offsets;
    while (true) {
        int64_t  min_k, max_k;
        uint64_t offset, size;
        uint32_t row_count, block_id;
        if (!meta.read(reinterpret_cast<char*>(&min_k),     sizeof(min_k)))     break;
        meta.read(reinterpret_cast<char*>(&max_k),     sizeof(max_k));
        meta.read(reinterpret_cast<char*>(&offset),    sizeof(offset));
        meta.read(reinterpret_cast<char*>(&size),      sizeof(size));
        meta.read(reinterpret_cast<char*>(&row_count), sizeof(row_count));
        meta.read(reinterpret_cast<char*>(&block_id),  sizeof(block_id));
        offsets.push_back(offset);
    }

    ASSERT_GT(offsets.size(), 1u);
    for (size_t i = 1; i < offsets.size(); ++i) {
        EXPECT_LE(offsets[i - 1], offsets[i])
            << "Block offsets must be non-decreasing (monotonic)";
    }
}

// ---------------------------------------------------------------------------
// 11. Full roundtrip with N rows via direct SSTableRowCursor
// ---------------------------------------------------------------------------

TEST_F(RowSSTRoundtripTest, DirectSSTableRowCursor_FullRoundtrip) {
    auto schema = make_schema_int();
    const int N = 100;

    RowSSTableBuilder builder(schema, sst_dir_);
    for (int i = 1; i <= N; ++i)
        builder.add(make_int_row(static_cast<Key>(i), i * 3));
    auto result = builder.finish();

    // Use reader to get metadata
    auto reader = std::make_shared<read::sstable::SSTableReader>(sst_dir_.string());
    auto all_meta = reader->read_row_metadata_range(0, result.num_blocks);

    auto types = schema_types(schema);
    cursor::SSTableRowCursor cursor(
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
        EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), (count + 1) * 3);
        cursor.next();
        ++count;
    }
    EXPECT_EQ(count, N);
}

// ---------------------------------------------------------------------------
// 12. Large string values don't corrupt adjacent rows
// ---------------------------------------------------------------------------

TEST_F(RowSSTRoundtripTest, LargeStringValues_DoNotCorruptAdjacentRows) {
    auto schema = make_schema_all_types();
    const std::string big(1024, 'A');
    const std::string small("x");

    RowSSTableBuilder builder(schema, sst_dir_);
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
