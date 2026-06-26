#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "storage/read/sstable/column_block_meta.hpp"
#include "storage/read/sstable/row_block_meta.hpp"
#include "storage/read/sstable/sstable_block_cache.hpp"
#include "storage/read/sstable/sstable_reader.hpp"

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

    void write_data(const std::vector<char>& bytes) const {
        std::ofstream out(path_ / "data.sst", std::ios::binary);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

private:
    void cleanup() const {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

private:
    std::filesystem::path path_;
};

RowBlockMeta row_block(
    std::size_t block_id,
    std::uint64_t offset,
    std::uint64_t size
) {
    return RowBlockMeta{
        .min_key = 0,
        .max_key = 0,
        .offset = offset,
        .size = size,
        .row_count = 0,
        .block_id = block_id
    };
}

ColumnBlockMeta column_block(
    std::size_t block_id,
    std::size_t column_idx,
    std::uint64_t offset,
    std::uint64_t size
) {
    return ColumnBlockMeta{
        .min_key = 0,
        .max_key = 0,
        .offset = offset,
        .size = size,
        .values_count = 0,
        .block_id = block_id,
        .column_idx = column_idx
    };
}

}

TEST(SSTableBlockCacheTest, CachesRowBlockAfterFirstRead) {
    TempSSTableDir dir("htap_sstable_block_cache_row_hit");
    dir.write_data({'a', 'b', 'c', 'd'});

    auto cache = std::make_shared<SSTableBlockCache>(1024);
    SSTableReader reader(dir.path(), cache, 42);

    const auto block = row_block(7, 0, 4);

    const auto first = reader.read_block(block);
    const auto second = reader.read_block(block);

    EXPECT_EQ(first, second);
    EXPECT_EQ(first, std::vector<char>({'a', 'b', 'c', 'd'}));

    const auto stats = cache->stats();
    EXPECT_EQ(stats.misses, 1u);
    EXPECT_EQ(stats.hits, 1u);
    EXPECT_EQ(stats.cached_blocks, 1u);
    EXPECT_EQ(stats.cached_bytes, 4u);
}

TEST(SSTableBlockCacheTest, SeparatesColumnBlocksByColumnIndex) {
    TempSSTableDir dir("htap_sstable_block_cache_column_key");
    dir.write_data({'a', 'b', 'c', 'd', 'e', 'f'});

    auto cache = std::make_shared<SSTableBlockCache>(1024);
    SSTableReader reader(dir.path(), cache, 42);

    const auto first_column = column_block(3, 1, 0, 3);
    const auto second_column = column_block(3, 2, 3, 3);

    EXPECT_EQ(reader.read_block(first_column), std::vector<char>({'a', 'b', 'c'}));
    EXPECT_EQ(reader.read_block(second_column), std::vector<char>({'d', 'e', 'f'}));
    EXPECT_EQ(reader.read_block(first_column), std::vector<char>({'a', 'b', 'c'}));
    EXPECT_EQ(reader.read_block(second_column), std::vector<char>({'d', 'e', 'f'}));

    const auto stats = cache->stats();
    EXPECT_EQ(stats.misses, 2u);
    EXPECT_EQ(stats.hits, 2u);
    EXPECT_EQ(stats.cached_blocks, 2u);
}

TEST(SSTableBlockCacheTest, EvictsLeastRecentlyUsedBlockByByteBudget) {
    TempSSTableDir dir("htap_sstable_block_cache_evicts_lru");
    dir.write_data({'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'});

    auto cache = std::make_shared<SSTableBlockCache>(4);
    SSTableReader reader(dir.path(), cache, 42);

    const auto first_block = row_block(1, 0, 4);
    const auto second_block = row_block(2, 4, 4);

    EXPECT_EQ(reader.read_block(first_block), std::vector<char>({'a', 'b', 'c', 'd'}));
    EXPECT_EQ(reader.read_block(second_block), std::vector<char>({'e', 'f', 'g', 'h'}));
    EXPECT_EQ(reader.read_block(first_block), std::vector<char>({'a', 'b', 'c', 'd'}));

    const auto stats = cache->stats();
    EXPECT_EQ(stats.misses, 3u);
    EXPECT_EQ(stats.hits, 0u);
    EXPECT_EQ(stats.cached_blocks, 1u);
    EXPECT_EQ(stats.cached_bytes, 4u);
}

TEST(SSTableBlockCacheTest, SeparatesBlocksBySSTableId) {
    TempSSTableDir first_dir("htap_sstable_block_cache_first_sst");
    TempSSTableDir second_dir("htap_sstable_block_cache_second_sst");
    first_dir.write_data({'a'});
    second_dir.write_data({'z'});

    auto cache = std::make_shared<SSTableBlockCache>(1024);
    SSTableReader first_reader(first_dir.path(), cache, 1);
    SSTableReader second_reader(second_dir.path(), cache, 2);

    const auto block = row_block(0, 0, 1);

    EXPECT_EQ(first_reader.read_block(block), std::vector<char>({'a'}));
    EXPECT_EQ(second_reader.read_block(block), std::vector<char>({'z'}));
    EXPECT_EQ(first_reader.read_block(block), std::vector<char>({'a'}));
    EXPECT_EQ(second_reader.read_block(block), std::vector<char>({'z'}));

    const auto stats = cache->stats();
    EXPECT_EQ(stats.misses, 2u);
    EXPECT_EQ(stats.hits, 2u);
}
