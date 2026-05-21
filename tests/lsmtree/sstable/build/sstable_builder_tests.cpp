#include <gtest/gtest.h>

#include <fstream>
#include <vector>
#include <cstdint>
#include <limits>

#include "lsmtree/sstable/build/sstable_builder.hpp"
#include "lsmtree/sstable/build/sst_footer.hpp"
#include "storage/model/schema_builder.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

// -----------------------------
// test helpers
// -----------------------------

static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    EXPECT_TRUE(file.is_open());

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return buffer;
}

static SSTFooter read_footer(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    EXPECT_TRUE(file.is_open());

    file.seekg(-(sizeof(uint32_t)
                + sizeof(uint32_t)
                + sizeof(uint64_t)
                + sizeof(uint64_t)
                + sizeof(uint64_t)
                + sizeof(uint8_t)),
               std::ios::end);

    SSTFooter footer{};

    file.read(reinterpret_cast<char*>(&footer.magic), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&footer.num_blocks), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&footer.meta_offset), sizeof(uint64_t));
    file.read(reinterpret_cast<char*>(&footer.min_key), sizeof(uint64_t));
    file.read(reinterpret_cast<char*>(&footer.max_key), sizeof(uint64_t));
    file.read(reinterpret_cast<char*>(&footer.layout_type), sizeof(uint8_t));

    return footer;
}

static std::vector<RowBlockMeta> read_meta(const std::string& path, const SSTFooter& footer) {
    std::ifstream file(path, std::ios::binary);
    EXPECT_TRUE(file.is_open());

    file.seekg(footer.meta_offset);

    std::vector<RowBlockMeta> meta(footer.num_blocks);

    for (auto& m : meta) {
        file.read(reinterpret_cast<char*>(&m.min_key), sizeof(int64_t));
        file.read(reinterpret_cast<char*>(&m.max_key), sizeof(int64_t));
        file.read(reinterpret_cast<char*>(&m.row_count), sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(&m.offset), sizeof(uint64_t));
        file.read(reinterpret_cast<char*>(&m.size_bytes), sizeof(uint64_t));
        file.read(reinterpret_cast<char*>(&m.block_id), sizeof(uint32_t));
    }

    return meta;
}

// -----------------------------
// test schema
// -----------------------------

static Schema make_schema() {
    return SchemaBuilder()
        .add_column("id", ValueType::INT64, true, false)
        .add_column("value", ValueType::INT64, false, true)
        .build();
}

static Row make_row(int64_t key) {
    Row r(2);

    r[0] = Value(key);
    r[1] = Value(key * 10);

    return r;
}

// -----------------------------
// tests
// -----------------------------

TEST(SSTableBuilderTest, CreatesFileAndFooterIsValid) {
    std::string path = "test_footer.sst";

    {
        Schema schema = make_schema();
        SSTableBuilder builder(schema, path);

        for (int i = 0; i < 1000; i++) {
            builder.add(make_row(i));
        }

        builder.finish();
    }

    SSTFooter footer = read_footer(path);

    EXPECT_EQ(footer.magic, SST_MAGIC);
    EXPECT_EQ(footer.layout_type, ROW_LAYOUT);
    EXPECT_EQ(footer.min_key, 0);
    EXPECT_EQ(footer.max_key, 999);
    EXPECT_GT(footer.num_blocks, 0);
}

TEST(SSTableBuilderTest, MetaOffsetIsValidAndReadable) {
    std::string path = "test_meta.sst";

    {
        Schema schema = make_schema();
        SSTableBuilder builder(schema, path);

        for (int i = 0; i < 500; i++) {
            builder.add(make_row(i));
        }

        builder.finish();
    }

    SSTFooter footer = read_footer(path);
    auto meta = read_meta(path, footer);

    EXPECT_EQ(meta.size(), footer.num_blocks);

    for (const auto& m : meta) {
        EXPECT_LE(m.min_key, m.max_key);
        EXPECT_GT(m.size_bytes, 0);
    }
}

TEST(SSTableBuilderTest, BlockOffsetsAreMonotonic) {
    std::string path = "test_offsets.sst";

    {
        Schema schema = make_schema();
        SSTableBuilder builder(schema, path);

        for (int i = 0; i < 2000; i++) {
            builder.add(make_row(i));
        }

        builder.finish();
    }

    SSTFooter footer = read_footer(path);
    auto meta = read_meta(path, footer);

    uint64_t last_offset = 0;

    for (const auto& m : meta) {
        EXPECT_GE(m.offset, last_offset);
        last_offset = m.offset;
    }
}

TEST(SSTableBuilderTest, MultipleBlocksExist) {
    std::string path = "test_blocks.sst";

    {
        Schema schema = make_schema();
        SSTableBuilder builder(schema, path);

        for (int i = 0; i < 5000; i++) {
            builder.add(make_row(i));
        }

        builder.finish();
    }

    SSTFooter footer = read_footer(path);

    EXPECT_GT(footer.num_blocks, 1);
}

TEST(SSTableBuilderTest, FileIsNonEmpty) {
    std::string path = "test_nonempty.sst";

    {
        Schema schema = make_schema();
        SSTableBuilder builder(schema, path);

        for (int i = 0; i < 100; i++) {
            builder.add(make_row(i));
        }

        builder.finish();
    }

    auto data = read_file(path);

    EXPECT_GT(data.size(), sizeof(SSTFooter));
}

TEST(SSTableBuilderTest, BlockMetaMatchesExpectedRange) {
    Schema schema = make_schema();
    std::string path = "range_test.sst";

    {
        SSTableBuilder builder(schema, path);

        for (int i = 0; i < 500; i++) {
            builder.add(make_row(i));
        }

        builder.finish();
    }

    auto footer = read_footer(path);
    auto meta = read_meta(path, footer);

    for (const auto& m : meta) {
        EXPECT_LE(m.min_key, m.max_key);
        EXPECT_GE(m.min_key, 0);
        EXPECT_LE(m.max_key, 499);
    }
}

TEST(SSTableBuilderTest, BlocksAreCreatedWhenSizeGrows) {
    Schema schema = make_schema();
    std::string path = "split_test.sst";

    {
        SSTableBuilder builder(schema, path);

        // много строк чтобы гарантировать split
        for (int i = 0; i < 10000; i++) {
            builder.add(make_row(i));
        }

        builder.finish();
    }

    auto footer = read_footer(path);

    EXPECT_GT(footer.num_blocks, 1);
}

TEST(SSTableBuilderTest, FileGrowsReasonably) {
    Schema schema = make_schema();
    std::string path = "size_test.sst";

    {
        SSTableBuilder builder(schema, path);

        for (int i = 0; i < 1000; i++) {
            builder.add(make_row(i));
        }

        builder.finish();
    }

    auto data = read_file(path);

    EXPECT_GT(data.size(), 1000); // очень грубый sanity check
}
