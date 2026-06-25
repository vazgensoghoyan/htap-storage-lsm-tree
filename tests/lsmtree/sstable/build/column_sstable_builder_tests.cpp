#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <vector>
#include <optional>

#include "lsmtree/sstable/build/column_sstable_builder.hpp"
#include "lsmtree/sstable/format/sst_layout.hpp"
#include "lsmtree/sstable/sstable_paths.hpp"
#include "storage/api/types.hpp"
#include "storage/model/schema_builder.hpp"
#include "storage/read/sstable/sstable_reader.hpp"
#include "storage/read/sstable/key_range.hpp"
#include "storage/cursor/sstable_column_cursor.hpp"
#include "storage/read/sstable/sparse_block_selector.hpp"
#include "lsmtree/sstable/metadata/sstable_info.hpp"
#include "storage/read/sstable/sstable_block_cache.hpp"
#include "storage/read/sstable/sstable_cursor_factory.hpp"
#include "storage/read/sstable/sstable_metadata_cache.hpp"

namespace fs = std::filesystem;

using namespace htap::storage;
using namespace htap::lsmtree::sstable;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

Schema make_schema() {
    return htap::storage::SchemaBuilder{}
        .add_column("id",    ValueType::INT64,   /*is_key=*/true,  /*nullable=*/false)
        .add_column("value", ValueType::INT64,   /*is_key=*/false, /*nullable=*/true)
        .add_column("score", ValueType::DOUBLE,  /*is_key=*/false, /*nullable=*/true)
        .build();
}

Row make_row(Key key, int64_t value, double score) {
    Row row(3);
    row[0] = key;
    row[1] = value;
    row[2] = score;
    return row;
}

Row make_row_nullable(Key key, std::optional<int64_t> value, std::optional<double> score) {
    Row row(3);
    row[0] = key;
    if (value) row[1] = *value; else row[1] = std::nullopt;
    if (score) row[2] = *score; else row[2] = std::nullopt;
    return row;
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

class ColumnSSTableBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "htap_col_sst_test";
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
// Tests
// ---------------------------------------------------------------------------

TEST_F(ColumnSSTableBuilderTest, EmptyThrows) {
    auto schema = make_schema();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    EXPECT_THROW(builder.finish(), std::runtime_error);
}

TEST_F(ColumnSSTableBuilderTest, SingleRow) {
    auto schema = make_schema();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_row(42, 100, 3.14));
    auto result = builder.finish();

    EXPECT_EQ(result.min_key, 42);
    EXPECT_EQ(result.max_key, 42);
    EXPECT_EQ(result.num_blocks, 1u);

    EXPECT_TRUE(fs::exists(sst_dir_ / "data.sst"));
    EXPECT_TRUE(fs::exists(sst_dir_ / "meta.bin"));
    EXPECT_TRUE(fs::exists(sst_dir_ / "info.bin"));
}

TEST_F(ColumnSSTableBuilderTest, MultipleRowsMinMaxCorrect) {
    auto schema = make_schema();
    ColumnSSTableBuilder builder(schema, sst_dir_);

    for (Key k = 1; k <= 10; ++k) {
        builder.add(make_row(k, k * 10, static_cast<double>(k) * 0.1));
    }

    auto result = builder.finish();
    EXPECT_EQ(result.min_key, 1);
    EXPECT_EQ(result.max_key, 10);
}

TEST_F(ColumnSSTableBuilderTest, UnsortedRowsThrow) {
    auto schema = make_schema();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_row(10, 1, 1.0));
    EXPECT_THROW(builder.add(make_row(5, 2, 2.0)), std::runtime_error);
}

TEST_F(ColumnSSTableBuilderTest, FinishTwiceThrows) {
    auto schema = make_schema();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_row(1, 10, 1.0));
    builder.finish();
    EXPECT_THROW(builder.finish(), std::runtime_error);
}

TEST_F(ColumnSSTableBuilderTest, InfoFileLayoutIsColumn) {
    auto schema = make_schema();
    ColumnSSTableBuilder builder(schema, sst_dir_);
    builder.add(make_row(1, 10, 1.0));
    builder.finish();

    // info.bin: uint32 num_blocks, int64 min_key, int64 max_key, uint8 layout
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

TEST_F(ColumnSSTableBuilderTest, RoundtripViaColumnCursor) {
    auto schema = make_schema();
    const int N = 50;

    {
        ColumnSSTableBuilder builder(schema, sst_dir_);
        for (int i = 1; i <= N; ++i) {
            builder.add(make_row(static_cast<Key>(i), i * 7, i * 0.5));
        }
        builder.finish();
    }

    // Читаем через sstable_cursor_factory
    std::vector<ValueType> schema_types;
    for (const auto& col : schema.columns()) {
        schema_types.push_back(col.type);
    }

    SSTableInfo info{
        .id              = 0,
        .path            = sst_dir_.string(),
        .level           = 1,
        .min_key         = 1,
        .max_key         = static_cast<Key>(N),
        .file_size_bytes = 0,
        .num_blocks      = 1,
        .layout          = SSTLayout::COLUMN
    };

    read::sstable::KeyRange range{std::nullopt, std::nullopt};
    std::vector<std::size_t> projection = {0, 1, 2};

    auto cursor = make_cursor(info, range, schema_types, projection);
    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());

    int count = 0;
    while (cursor->valid()) {
        Key k = cursor->key();
        auto val = cursor->value(1);
        auto score = cursor->value(2);

        EXPECT_EQ(k, static_cast<Key>(count + 1));
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(std::get<int64_t>(*val), (count + 1) * 7);
        ASSERT_TRUE(score.has_value());
        EXPECT_DOUBLE_EQ(std::get<double>(*score), (count + 1) * 0.5);

        cursor->next();
        ++count;
    }

    EXPECT_EQ(count, N);
}

TEST_F(ColumnSSTableBuilderTest, RoundtripWithNullValues) {
    auto schema = make_schema();

    {
        ColumnSSTableBuilder builder(schema, sst_dir_);
        builder.add(make_row_nullable(1, 10,   std::nullopt));
        builder.add(make_row_nullable(2, std::nullopt, 2.5));
        builder.add(make_row_nullable(3, 30,   3.0));
        builder.finish();
    }

    std::vector<ValueType> schema_types;
    for (const auto& col : schema.columns()) {
        schema_types.push_back(col.type);
    }

    SSTableInfo info{
        .id              = 0,
        .path            = sst_dir_.string(),
        .level           = 1,
        .min_key         = 1,
        .max_key         = 3,
        .file_size_bytes = 0,
        .num_blocks      = 1,
        .layout          = SSTLayout::COLUMN
    };

    read::sstable::KeyRange range{std::nullopt, std::nullopt};
    std::vector<std::size_t> projection = {0, 1, 2};

    auto cursor = make_cursor(info, range, schema_types, projection);
    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());

    // row key=1: value=10, score=null
    EXPECT_EQ(cursor->key(), 1);
    EXPECT_TRUE(cursor->value(1).has_value());
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 10);
    EXPECT_FALSE(cursor->value(2).has_value());
    cursor->next();

    // row key=2: value=null, score=2.5
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 2);
    EXPECT_FALSE(cursor->value(1).has_value());
    EXPECT_TRUE(cursor->value(2).has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*cursor->value(2)), 2.5);
    cursor->next();

    // row key=3: value=30, score=3.0
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 3);
    EXPECT_TRUE(cursor->value(1).has_value());
    EXPECT_EQ(std::get<int64_t>(*cursor->value(1)), 30);
    cursor->next();

    EXPECT_FALSE(cursor->valid());
}

TEST_F(ColumnSSTableBuilderTest, MultipleBlocksRoundtrip) {
    auto schema = make_schema();
    // Пишем больше TARGET_BLOCK_ROWS строк, чтобы гарантированно получить >1 блока
    const int N = 300;

    {
        ColumnSSTableBuilder builder(schema, sst_dir_);
        for (int i = 1; i <= N; ++i) {
            builder.add(make_row(static_cast<Key>(i), i, static_cast<double>(i)));
        }
        auto result = builder.finish();
        EXPECT_GT(result.num_blocks, 1u);
    }

    std::vector<ValueType> schema_types;
    for (const auto& col : schema.columns()) {
        schema_types.push_back(col.type);
    }

    SSTableInfo info{
        .id              = 0,
        .path            = sst_dir_.string(),
        .level           = 1,
        .min_key         = 1,
        .max_key         = static_cast<Key>(N),
        .file_size_bytes = 0,
        .num_blocks      = 0, // будет определено из cursor factory
        .layout          = SSTLayout::COLUMN
    };

    // Читаем info.bin для num_blocks
    {
        std::ifstream inf(sst_dir_ / "info.bin", std::ios::binary);
        uint32_t nb = 0;
        inf.read(reinterpret_cast<char*>(&nb), sizeof(nb));
        info.num_blocks = nb;
    }

    read::sstable::KeyRange range{std::nullopt, std::nullopt};
    std::vector<std::size_t> projection = {0, 1, 2};

    auto cursor = make_cursor(info, range, schema_types, projection);
    ASSERT_NE(cursor, nullptr);

    int count = 0;
    while (cursor->valid()) {
        EXPECT_EQ(cursor->key(), static_cast<Key>(count + 1));
        cursor->next();
        ++count;
    }
    EXPECT_EQ(count, N);
}

TEST_F(ColumnSSTableBuilderTest, WritesNumericStatsFileReadableBySSTableReader) {
    auto schema = make_schema();
    const int N = 300;

    SSTableBuildResult build_result;
    {
        ColumnSSTableBuilder builder(schema, sst_dir_, /*sparse_index_step=*/1);
        for (int i = 1; i <= N; ++i) {
            builder.add(make_row(static_cast<Key>(i), i * 10, static_cast<double>(i) * 0.25));
        }
        build_result = builder.finish();
    }

    ASSERT_TRUE(fs::exists(sst_dir_ / "stats.bin"));
    ASSERT_GT(build_result.num_blocks, 1u);

    read::sstable::SSTableReader reader(sst_dir_);
    const auto metadata = reader.read_column_metadata_range(
        0,
        build_result.num_blocks,
        static_cast<std::uint32_t>(schema.size())
    );

    const auto all_stats = reader.read_numeric_stats_range(
        0,
        build_result.num_blocks,
        {0, 1, 2}
    );

    EXPECT_FALSE(all_stats.by_column.contains(0));
    ASSERT_TRUE(all_stats.by_column.contains(1));
    ASSERT_TRUE(all_stats.by_column.contains(2));
    ASSERT_EQ(all_stats.by_column.at(1).size(), build_result.num_blocks);
    ASSERT_EQ(all_stats.by_column.at(2).size(), build_result.num_blocks);

    for (std::uint32_t block_id = 0; block_id < build_result.num_blocks; ++block_id) {
        const auto key_meta_it = std::find_if(
            metadata.begin(),
            metadata.end(),
            [block_id](const auto& meta) {
                return meta.block_id == block_id && meta.column_idx == KEY_COLUMN_INDEX;
            }
        );
        ASSERT_NE(key_meta_it, metadata.end());

        const auto& value_stats = all_stats.by_column.at(1)[block_id];
        EXPECT_EQ(value_stats.column_idx, 1u);
        EXPECT_EQ(value_stats.type, ValueType::INT64);
        EXPECT_TRUE(value_stats.has_value);
        EXPECT_EQ(std::get<std::int64_t>(value_stats.min_value), key_meta_it->min_key * 10);
        EXPECT_EQ(std::get<std::int64_t>(value_stats.max_value), key_meta_it->max_key * 10);

        const auto& score_stats = all_stats.by_column.at(2)[block_id];
        EXPECT_EQ(score_stats.column_idx, 2u);
        EXPECT_EQ(score_stats.type, ValueType::DOUBLE);
        EXPECT_TRUE(score_stats.has_value);
        EXPECT_DOUBLE_EQ(std::get<double>(score_stats.min_value), static_cast<double>(key_meta_it->min_key) * 0.25);
        EXPECT_DOUBLE_EQ(std::get<double>(score_stats.max_value), static_cast<double>(key_meta_it->max_key) * 0.25);
    }

    const auto second_block_score = reader.read_numeric_stats_range(1, 1, {2});
    ASSERT_TRUE(second_block_score.by_column.contains(2));
    ASSERT_EQ(second_block_score.by_column.at(2).size(), 1u);

    const auto missing_column = reader.read_numeric_stats_range(0, 1, {42});
    EXPECT_TRUE(missing_column.by_column.empty());
}
