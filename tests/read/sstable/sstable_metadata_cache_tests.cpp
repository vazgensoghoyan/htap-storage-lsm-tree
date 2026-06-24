#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "lsmtree/sstable/build/sstable_builder.hpp"
#include "lsmtree/sstable/metadata/sstable_info.hpp"
#include "storage/model/schema_builder.hpp"
#include "storage/read/sstable/sstable_metadata_cache.hpp"
#include "storage/read/sstable/sstable_reader.hpp"

using namespace htap::lsmtree::sstable;
using namespace htap::storage;
using namespace htap::storage::read::sstable;

namespace {

class TempSSTableDir {
public:
    explicit TempSSTableDir(std::string name)
        : path_(std::filesystem::temp_directory_path() / std::move(name)) {
        cleanup();
        std::filesystem::create_directories(path_);
    }

    ~TempSSTableDir() {
        cleanup();
    }

    const std::filesystem::path& path() const {
        return path_;
    }

private:
    void cleanup() const {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

private:
    std::filesystem::path path_;
};

Schema make_schema() {
    return SchemaBuilder()
        .add_column("id", ValueType::INT64, true, false)
        .add_column("age", ValueType::INT64, false, true)
        .add_column("score", ValueType::DOUBLE, false, true)
        .add_column("name", ValueType::STRING, false, true)
        .build();
}

Row make_row(std::int64_t id) {
    Row row;
    row.push_back(id);
    row.push_back(id * 3);
    row.push_back(static_cast<double>(id) / 2.0);
    row.push_back(std::string(48, 'x'));
    return row;
}

SSTableInfo make_info(const std::filesystem::path& path, const SSTableBuildResult& result) {
    return SSTableInfo{
        .id = 7,
        .path = path.string(),
        .level = 0,
        .min_key = result.min_key,
        .max_key = result.max_key,
        .file_size_bytes = 0,
        .num_blocks = result.num_blocks,
        .layout = SSTLayout::ROW
    };
}

}

TEST(SSTableMetadataCacheTest, ReadsRowMetadataThroughPagedCache) {
    TempSSTableDir dir("htap_sstable_metadata_cache_row_metadata");
    const auto schema = make_schema();

    SSTableBuilder builder(schema, dir.path(), 1);
    for (std::int64_t id = 0; id < 400; ++id) {
        builder.add(make_row(id));
    }

    const auto build_result = builder.finish();
    ASSERT_GE(build_result.num_blocks, 3u);

    SSTableReader reader(dir.path());
    const auto expected = reader.read_row_metadata_range(1, build_result.num_blocks - 1);

    SSTableMetadataCache cache(
        make_info(dir.path(), build_result),
        static_cast<std::uint32_t>(schema.size()),
        2,
        256
    );

    const auto actual = cache.read_row_metadata_range(1, build_result.num_blocks - 1);

    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        EXPECT_EQ(actual[i].min_key, expected[i].min_key);
        EXPECT_EQ(actual[i].max_key, expected[i].max_key);
        EXPECT_EQ(actual[i].block_id, expected[i].block_id);
    }
}

TEST(SSTableMetadataCacheTest, ReadsNumericStatsThroughPagedCache) {
    TempSSTableDir dir("htap_sstable_metadata_cache_numeric_stats");
    const auto schema = make_schema();

    SSTableBuilder builder(schema, dir.path(), 1);
    for (std::int64_t id = 0; id < 500; ++id) {
        builder.add(make_row(id));
    }

    const auto build_result = builder.finish();
    ASSERT_GE(build_result.num_blocks, 3u);

    SSTableReader reader(dir.path());
    const auto expected = reader.read_numeric_stats_range(1, build_result.num_blocks - 1, {1, 2});

    SSTableMetadataCache cache(
        make_info(dir.path(), build_result),
        static_cast<std::uint32_t>(schema.size()),
        2,
        192
    );

    const auto actual = cache.read_numeric_stats_range(1, build_result.num_blocks - 1, {1, 2});

    ASSERT_TRUE(actual.by_column.contains(1));
    ASSERT_TRUE(actual.by_column.contains(2));
    ASSERT_EQ(actual.by_column.at(1).size(), expected.by_column.at(1).size());
    ASSERT_EQ(actual.by_column.at(2).size(), expected.by_column.at(2).size());

    for (std::size_t i = 0; i < actual.by_column.at(1).size(); ++i) {
        const auto& actual_age = actual.by_column.at(1)[i];
        const auto& expected_age = expected.by_column.at(1)[i];
        EXPECT_EQ(actual_age.has_value, expected_age.has_value);
        EXPECT_EQ(std::get<std::int64_t>(actual_age.min_value), std::get<std::int64_t>(expected_age.min_value));
        EXPECT_EQ(std::get<std::int64_t>(actual_age.max_value), std::get<std::int64_t>(expected_age.max_value));

        const auto& actual_score = actual.by_column.at(2)[i];
        const auto& expected_score = expected.by_column.at(2)[i];
        EXPECT_EQ(actual_score.has_value, expected_score.has_value);
        EXPECT_DOUBLE_EQ(std::get<double>(actual_score.min_value), std::get<double>(expected_score.min_value));
        EXPECT_DOUBLE_EQ(std::get<double>(actual_score.max_value), std::get<double>(expected_score.max_value));
    }
}

TEST(SSTableMetadataCacheTest, MissingStatsColumnIsCachedConservatively) {
    TempSSTableDir dir("htap_sstable_metadata_cache_missing_stats");
    const auto schema = make_schema();

    SSTableBuilder builder(schema, dir.path(), 1);
    for (std::int64_t id = 0; id < 100; ++id) {
        builder.add(make_row(id));
    }

    const auto build_result = builder.finish();

    SSTableMetadataCache cache(
        make_info(dir.path(), build_result),
        static_cast<std::uint32_t>(schema.size()),
        2,
        128
    );

    const auto first = cache.read_numeric_stats_range(0, 1, {99});
    const auto second = cache.read_numeric_stats_range(0, 1, {99});

    EXPECT_TRUE(first.by_column.empty());
    EXPECT_TRUE(second.by_column.empty());
}

TEST(SSTableMetadataCacheTest, MissingStatsFileReturnsEmptyStatsRange) {
    TempSSTableDir dir("htap_sstable_metadata_cache_missing_stats_file");
    {
        std::ofstream data(dir.path() / "data.sst", std::ios::binary);
    }

    SSTableInfo info{
        .id = 9,
        .path = dir.path().string(),
        .level = 0,
        .min_key = 0,
        .max_key = 0,
        .file_size_bytes = 0,
        .num_blocks = 1,
        .layout = SSTLayout::ROW
    };

    SSTableMetadataCache cache(info, 4, 2, 128);
    const auto stats = cache.read_numeric_stats_range(0, 1, {1});

    EXPECT_EQ(stats.first_block_id, 0u);
    EXPECT_EQ(stats.block_count, 1u);
    EXPECT_TRUE(stats.by_column.empty());
}
