#include <gtest/gtest.h>

#include <filesystem>
#include <vector>
#include <stdexcept>

#include "storage/model/schema_builder.hpp"

#include "lsmtree/sstable/format/sstable_info.hpp"
#include "lsmtree/sstable/format/sparse_index_entry.hpp"
#include "lsmtree/sstable/sstable_builder.hpp"

using namespace htap::lsmtree::sstable;
using namespace htap::storage;

// Test helpers

namespace {

Schema make_test_schema() {
    return SchemaBuilder()
        .add_column("id", ValueType::INT64, true, false)
        .add_column("some_int", ValueType::INT64, false, false)
        .add_column("name", ValueType::STRING, false, true)
        .build();
}

Row make_row(int64_t key) {
    Row row;

    row.push_back(key);
    row.push_back(int64_t(key * 10));
    row.push_back(std::string("value_" + std::to_string(key)));

    return row;
}

void cleanup(const std::filesystem::path& dir) {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
}

std::vector<format::SparseIndexEntry> read_index(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::vector<format::SparseIndexEntry> out;

    if (!in.is_open()) return out;

    while (true) {
        format::SparseIndexEntry e;
        if (!in.read(reinterpret_cast<char*>(&e.min_key), sizeof(e.min_key))) break;
        if (!in.read(reinterpret_cast<char*>(&e.block_id), sizeof(e.block_id))) break;
        out.push_back(e);
    }

    return out;
}

format::SSTableInfo read_info(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("cannot open info file");
    }

    format::SSTableInfo info;

    in.read(reinterpret_cast<char*>(&info.magic), sizeof(info.magic));
    in.read(reinterpret_cast<char*>(&info.num_blocks), sizeof(info.num_blocks));
    in.read(reinterpret_cast<char*>(&info.min_key), sizeof(info.min_key));
    in.read(reinterpret_cast<char*>(&info.max_key), sizeof(info.max_key));

    uint8_t layout;
    in.read(reinterpret_cast<char*>(&layout), sizeof(layout));
    info.layout_type = static_cast<format::SSTLayout>(layout);

    return info;
}

} // namespace

// 1. SST creation sanity

TEST(SSTableBuilderTest, CreatesAllFiles) {
    std::filesystem::path dir = "/tmp/sst_test_1";
    cleanup(dir);

    auto schema = make_test_schema();
    SSTableBuilder builder(schema, dir, 3);

    for (int i = 0; i < 50; ++i) {
        builder.add(make_row(i));
    }

    builder.finish();

    ASSERT_TRUE(std::filesystem::exists(dir / "data.sst"));
    ASSERT_TRUE(std::filesystem::exists(dir / "meta.bin"));
    ASSERT_TRUE(std::filesystem::exists(dir / "sparse.idx"));
    ASSERT_TRUE(std::filesystem::exists(dir / "info.bin"));
}

// 2. Sorted order enforcement

TEST(SSTableBuilderTest, EnforcesSortedInput) {
    std::filesystem::path dir = "/tmp/sst_test_2";
    cleanup(dir);

    auto schema = make_test_schema();
    SSTableBuilder builder(schema, dir, 2);

    builder.add(make_row(10));
    builder.add(make_row(20));

    EXPECT_THROW(builder.add(make_row(15)), std::runtime_error);
}

// 3. Global min/max correctness

TEST(SSTableBuilderTest, GlobalMinMaxCorrect) {
    std::filesystem::path dir = "/tmp/sst_test_3";
    cleanup(dir);

    auto schema = make_test_schema();
    SSTableBuilder builder(schema, dir, 2);

    for (int i = 100; i < 200; ++i) {
        builder.add(make_row(i));
    }

    builder.finish();

    auto info = read_info(dir / "info.bin");

    ASSERT_EQ(info.min_key, 100);
    ASSERT_EQ(info.max_key, 199);
}

// 4. Sparse index sanity

TEST(SSTableBuilderTest, SparseIndexIsValid) {
    std::filesystem::path dir = "/tmp/sst_test_4";
    cleanup(dir);

    auto schema = make_test_schema();
    SSTableBuilder builder(schema, dir, 5);

    for (int i = 0; i < 50; ++i) {
        builder.add(make_row(i));
    }

    builder.finish();

    auto index = read_index(dir / "sparse.idx");

    ASSERT_FALSE(index.empty());

    for (const auto& e : index) {
        ASSERT_TRUE(e.block_id % 5 == 0);
    }

    for (size_t i = 1; i < index.size(); ++i) {
        ASSERT_LT(index[i - 1].min_key, index[i].min_key);
    }
}

// 5. Multiple blocks created

TEST(SSTableBuilderTest, ProducesMultipleBlocks) {
    std::filesystem::path dir = "/tmp/sst_test_5";
    cleanup(dir);

    auto schema = make_test_schema();
    SSTableBuilder builder(schema, dir, 1);

    for (int i = 0; i < 5000; ++i) {
        builder.add(make_row(i));
    }

    builder.finish();

    auto info = read_info(dir / "info.bin");

    ASSERT_GT(info.num_blocks, 1);
}

// 6. Empty SST forbidden

TEST(SSTableBuilderTest, EmptySSTThrows) {
    std::filesystem::path dir = "/tmp/sst_test_6";
    cleanup(dir);

    auto schema = make_test_schema();
    SSTableBuilder builder(schema, dir, 2);

    EXPECT_THROW(builder.finish(), std::runtime_error);
}

// 7. Block monotonic offsets

TEST(SSTableBuilderTest, BlockOffsetsMonotonic) {
    std::filesystem::path dir = "/tmp/sst_test_7";
    cleanup(dir);

    auto schema = make_test_schema();
    SSTableBuilder builder(schema, dir, 3);

    for (int i = 0; i < 200; ++i) {
        builder.add(make_row(i));
    }

    builder.finish();

    std::ifstream meta(dir / "meta.bin", std::ios::binary);

    std::vector<uint64_t> offsets;

    while (true) {
        int64_t min_k, max_k;
        uint64_t offset;
        uint64_t size;
        uint32_t rows;
        uint32_t block_id;

        if (!meta.read(reinterpret_cast<char*>(&min_k), sizeof(min_k))) break;
        meta.read(reinterpret_cast<char*>(&max_k), sizeof(max_k));
        meta.read(reinterpret_cast<char*>(&offset), sizeof(offset));

        offsets.push_back(offset);

        meta.read(reinterpret_cast<char*>(&size), sizeof(size));
        meta.read(reinterpret_cast<char*>(&rows), sizeof(rows));
        meta.read(reinterpret_cast<char*>(&block_id), sizeof(block_id));
    }

    for (size_t i = 1; i < offsets.size(); ++i) {
        ASSERT_LE(offsets[i - 1], offsets[i]);
    }
}

// 8. Block splitting actually happens

TEST(SSTableBuilderTest, BlockSplittingWorks) {
    std::filesystem::path dir = "/tmp/sst_test_8";
    cleanup(dir);

    auto schema = make_test_schema();
    SSTableBuilder builder(schema, dir, 10);

    for (int i = 0; i < 20000; ++i) {
        builder.add(make_row(i));
    }

    builder.finish();

    auto info = read_info(dir / "info.bin");

    ASSERT_GT(info.num_blocks, 1);
}
