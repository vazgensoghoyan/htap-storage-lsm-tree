#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "lsmtree/sstable/build/row_sstable_builder.hpp"
#include "storage/model/schema_builder.hpp"
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
    row.push_back(id * 2);
    row.push_back(static_cast<double>(id) / 10.0);
    row.push_back(std::string(32, 'x'));
    return row;
}

}

TEST(NumericStatsReaderTest, ReadsStatsForRequestedColumnsAndRanges) {
    TempSSTableDir dir("htap_numeric_stats_reader_roundtrip");
    const auto schema = make_schema();

    RowSSTableBuilder builder(schema, dir.path(), 1);
    for (std::int64_t id = 0; id < 1500; ++id) {
        builder.add(make_row(id));
    }

    const auto build_result = builder.finish();
    ASSERT_TRUE(std::filesystem::exists(dir.path() / "stats.bin"));
    ASSERT_GE(build_result.num_blocks, 2u);

    SSTableReader reader(dir.path());
    const auto metadata = reader.read_row_metadata_range(0, build_result.num_blocks);

    const auto one_column = reader.read_numeric_stats_range(0, build_result.num_blocks, {1});
    ASSERT_TRUE(one_column.by_column.contains(1));
    EXPECT_FALSE(one_column.by_column.contains(2));
    ASSERT_EQ(one_column.by_column.at(1).size(), build_result.num_blocks);

    for (std::uint32_t block_id = 0; block_id < build_result.num_blocks; ++block_id) {
        const auto& block = metadata[block_id];
        const auto& stats = one_column.by_column.at(1)[block_id];
        EXPECT_EQ(stats.column_idx, 1u);
        EXPECT_EQ(stats.type, ValueType::INT64);
        EXPECT_TRUE(stats.has_value);
        EXPECT_EQ(std::get<std::int64_t>(stats.min_value), block.min_key * 2);
        EXPECT_EQ(std::get<std::int64_t>(stats.max_value), block.max_key * 2);
    }

    const auto two_columns = reader.read_numeric_stats_range(1, 1, {1, 2});
    ASSERT_TRUE(two_columns.by_column.contains(1));
    ASSERT_TRUE(two_columns.by_column.contains(2));
    ASSERT_EQ(two_columns.by_column.at(1).size(), 1u);
    ASSERT_EQ(two_columns.by_column.at(2).size(), 1u);

    const auto& second_block = metadata[1];
    const auto& age = two_columns.by_column.at(1).front();
    EXPECT_EQ(std::get<std::int64_t>(age.min_value), second_block.min_key * 2);
    EXPECT_EQ(std::get<std::int64_t>(age.max_value), second_block.max_key * 2);

    const auto& score = two_columns.by_column.at(2).front();
    EXPECT_EQ(score.type, ValueType::DOUBLE);
    EXPECT_DOUBLE_EQ(std::get<double>(score.min_value), static_cast<double>(second_block.min_key) / 10.0);
    EXPECT_DOUBLE_EQ(std::get<double>(score.max_value), static_cast<double>(second_block.max_key) / 10.0);

    const auto missing_column = reader.read_numeric_stats_range(0, 1, {42});
    EXPECT_TRUE(missing_column.by_column.empty());
}

TEST(NumericStatsReaderTest, MissingStatsFileReturnsEmptyRange) {
    TempSSTableDir dir("htap_numeric_stats_reader_missing_file");
    {
        std::ofstream data(dir.path() / "data.sst", std::ios::binary);
    }

    SSTableReader reader(dir.path());
    const auto stats = reader.read_numeric_stats_range(0, 1, {1});

    EXPECT_EQ(stats.first_block_id, 0u);
    EXPECT_EQ(stats.block_count, 1u);
    EXPECT_TRUE(stats.by_column.empty());
}
