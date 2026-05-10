#include "storage/cursor/sstable_column_cursor.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace htap::storage::cursor {
namespace {

class TempFile {
public:
    explicit TempFile(std::string name)
        : path_(std::filesystem::temp_directory_path() / std::move(name)) {
        std::filesystem::remove(path_);
    }

    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    const std::filesystem::path& path() const {
        return path_;
    }

    void write(const std::vector<char>& data) const {
        std::ofstream out(path_, std::ios::binary);
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }

    void touch() const {
        std::ofstream out(path_, std::ios::binary);
    }

private:
    std::filesystem::path path_;
};

void append_bytes(std::vector<char>& out, const void* data, std::size_t size) {
    const auto* ptr = static_cast<const char*>(data);
    out.insert(out.end(), ptr, ptr + size);
}

template <typename T>
void append_pod(std::vector<char>& out, const T& value) {
    append_bytes(out, &value, sizeof(T));
}

void append_string(std::vector<char>& out, const std::string& value) {
    const auto size = static_cast<std::uint32_t>(value.size());
    append_pod(out, size);
    out.insert(out.end(), value.begin(), value.end());
}

read::sstable::ColumnBlockMeta make_meta(
    Key min_key,
    Key max_key,
    std::uint64_t offset,
    std::uint64_t size,
    std::uint32_t values_count,
    std::size_t block_id,
    std::size_t column_idx
) {
    read::sstable::ColumnBlockMeta meta{};
    meta.min_key = min_key;
    meta.max_key = max_key;
    meta.offset = offset;
    meta.size = size;
    meta.values_count = values_count;
    meta.block_id = block_id;
    meta.column_idx = column_idx;
    return meta;
}

std::vector<char> make_key_block(const std::vector<Key>& keys) {
    std::vector<char> data;

    for (const auto key : keys) {
        append_pod(data, key);
    }

    return data;
}

std::vector<char> make_int64_column_block(
    const std::vector<std::optional<std::int64_t>>& values
) {
    std::vector<char> data;

    const std::size_t bitmap_size = (values.size() + 7) / 8;
    std::vector<std::uint8_t> bitmap(bitmap_size, 0);

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (!values[i].has_value()) {
            bitmap[i / 8] |= static_cast<std::uint8_t>(std::uint8_t{1} << (i % 8));
        }
    }

    data.insert(data.end(), bitmap.begin(), bitmap.end());

    for (const auto& value : values) {
        if (value.has_value()) {
            append_pod(data, *value);
        }
    }

    return data;
}

std::vector<char> make_double_column_block(
    const std::vector<std::optional<double>>& values
) {
    std::vector<char> data;

    const std::size_t bitmap_size = (values.size() + 7) / 8;
    std::vector<std::uint8_t> bitmap(bitmap_size, 0);

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (!values[i].has_value()) {
            bitmap[i / 8] |= static_cast<std::uint8_t>(std::uint8_t{1} << (i % 8));
        }
    }

    data.insert(data.end(), bitmap.begin(), bitmap.end());

    for (const auto& value : values) {
        if (value.has_value()) {
            append_pod(data, *value);
        }
    }

    return data;
}

std::vector<char> make_string_column_block(
    const std::vector<std::optional<std::string>>& values
) {
    std::vector<char> data;

    const std::size_t bitmap_size = (values.size() + 7) / 8;
    std::vector<std::uint8_t> bitmap(bitmap_size, 0);

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (!values[i].has_value()) {
            bitmap[i / 8] |= static_cast<std::uint8_t>(std::uint8_t{1} << (i % 8));
        }
    }

    data.insert(data.end(), bitmap.begin(), bitmap.end());

    for (const auto& value : values) {
        if (value.has_value()) {
            append_string(data, *value);
        }
    }

    return data;
}

struct ColumnFileBuilder {
    std::vector<char> file_data;
    std::vector<read::sstable::ColumnBlockMeta> blocks;

    void add_block(
        std::size_t block_id,
        std::size_t column_idx,
        Key min_key,
        Key max_key,
        std::uint32_t values_count,
        const std::vector<char>& block_data
    ) {
        const auto offset = static_cast<std::uint64_t>(file_data.size());

        file_data.insert(file_data.end(), block_data.begin(), block_data.end());

        blocks.push_back(make_meta(
            min_key,
            max_key,
            offset,
            static_cast<std::uint64_t>(block_data.size()),
            values_count,
            block_id,
            column_idx
        ));
    }
};

std::vector<ValueType> test_schema() {
    return {
        ValueType::INT64,   // 0 key
        ValueType::INT64,   // 1 age
        ValueType::DOUBLE,  // 2 score
        ValueType::STRING   // 3 name
    };
}

read::sstable::KeyRange range(std::optional<Key> from, std::optional<Key> to) {
    read::sstable::KeyRange result;
    result.from = from;
    result.to = to;
    return result;
}

} 

TEST(SSTableColumnCursorTest, EmptyBlocksProduceInvalidCursor) {
    TempFile file("htap_sstable_column_cursor_empty_blocks.bin");
    file.touch();

    SSTableColumnCursor cursor(
        file.path(),
        {},
        range(std::nullopt, std::nullopt),
        test_schema(),
        {0, 1, 2, 3}
    );

    EXPECT_FALSE(cursor.valid());
}

TEST(SSTableColumnCursorTest, ReadsProjectedColumnsAndNullsInRange) {
    TempFile file("htap_sstable_column_cursor_projected_columns.bin");

    ColumnFileBuilder builder;

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/0,
        /*min_key=*/1,
        /*max_key=*/4,
        /*values_count=*/3,
        make_key_block({1, 2, 3})
    );

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/1,
        /*min_key=*/1,
        /*max_key=*/4,
        /*values_count=*/3,
        make_int64_column_block({10, std::nullopt, 30})
    );

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/2,
        /*min_key=*/1,
        /*max_key=*/4,
        /*values_count=*/3,
        make_double_column_block({1.5, 2.5, std::nullopt})
    );

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/3,
        /*min_key=*/1,
        /*max_key=*/4,
        /*values_count=*/3,
        make_string_column_block({"a", "bb", "ccc"})
    );

    file.write(builder.file_data);

    SSTableColumnCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(2, 4),
        test_schema(),
        {0, 1, 2, 3}
    );

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 2);

    EXPECT_FALSE(cursor.value(1).has_value());

    ASSERT_TRUE(cursor.value(2).has_value());
    EXPECT_EQ(std::get<double>(*cursor.value(2)), 2.5);

    ASSERT_TRUE(cursor.value(3).has_value());
    EXPECT_EQ(std::get<std::string>(*cursor.value(3)), "bb");

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 3);

    ASSERT_TRUE(cursor.value(1).has_value());
    EXPECT_EQ(std::get<std::int64_t>(*cursor.value(1)), 30);

    EXPECT_FALSE(cursor.value(2).has_value());

    ASSERT_TRUE(cursor.value(3).has_value());
    EXPECT_EQ(std::get<std::string>(*cursor.value(3)), "ccc");

    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(SSTableColumnCursorTest, RespectsUpperExclusiveRangeBoundary) {
    TempFile file("htap_sstable_column_cursor_upper_exclusive.bin");

    ColumnFileBuilder builder;

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/0,
        /*min_key=*/1,
        /*max_key=*/5,
        /*values_count=*/4,
        make_key_block({1, 2, 3, 4})
    );

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/1,
        /*min_key=*/1,
        /*max_key=*/5,
        /*values_count=*/4,
        make_int64_column_block({10, 20, 30, 40})
    );

    file.write(builder.file_data);

    SSTableColumnCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(2, 4),
        test_schema(),
        {0, 1}
    );

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 2);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 3);

    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(SSTableColumnCursorTest, SupportsOpenEndedRanges) {
    TempFile file("htap_sstable_column_cursor_open_ended_ranges.bin");

    ColumnFileBuilder builder;

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/0,
        /*min_key=*/1,
        /*max_key=*/5,
        /*values_count=*/4,
        make_key_block({1, 2, 3, 4})
    );

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/1,
        /*min_key=*/1,
        /*max_key=*/5,
        /*values_count=*/4,
        make_int64_column_block({10, 20, 30, 40})
    );

    file.write(builder.file_data);

    {
        SSTableColumnCursor cursor(
            file.path(),
            builder.blocks,
            range(std::nullopt, 3),
            test_schema(),
            {0, 1}
        );

        ASSERT_TRUE(cursor.valid());
        EXPECT_EQ(cursor.key(), 1);

        cursor.next();

        ASSERT_TRUE(cursor.valid());
        EXPECT_EQ(cursor.key(), 2);

        cursor.next();

        EXPECT_FALSE(cursor.valid());
    }

    {
        SSTableColumnCursor cursor(
            file.path(),
            builder.blocks,
            range(3, std::nullopt),
            test_schema(),
            {0, 1}
        );

        ASSERT_TRUE(cursor.valid());
        EXPECT_EQ(cursor.key(), 3);

        cursor.next();

        ASSERT_TRUE(cursor.valid());
        EXPECT_EQ(cursor.key(), 4);

        cursor.next();

        EXPECT_FALSE(cursor.valid());
    }
}

TEST(SSTableColumnCursorTest, SupportsMultipleLogicalBlocks) {
    TempFile file("htap_sstable_column_cursor_multiple_logical_blocks.bin");

    ColumnFileBuilder builder;

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/0,
        /*min_key=*/1,
        /*max_key=*/3,
        /*values_count=*/2,
        make_key_block({1, 2})
    );

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/1,
        /*min_key=*/1,
        /*max_key=*/3,
        /*values_count=*/2,
        make_int64_column_block({10, 20})
    );

    builder.add_block(
        /*block_id=*/1,
        /*column_idx=*/0,
        /*min_key=*/3,
        /*max_key=*/5,
        /*values_count=*/2,
        make_key_block({3, 4})
    );

    builder.add_block(
        /*block_id=*/1,
        /*column_idx=*/1,
        /*min_key=*/3,
        /*max_key=*/5,
        /*values_count=*/2,
        make_int64_column_block({30, 40})
    );

    file.write(builder.file_data);

    SSTableColumnCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(1, 5),
        test_schema(),
        {0, 1}
    );

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 1);
    EXPECT_EQ(std::get<std::int64_t>(*cursor.value(1)), 10);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 2);
    EXPECT_EQ(std::get<std::int64_t>(*cursor.value(1)), 20);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 3);
    EXPECT_EQ(std::get<std::int64_t>(*cursor.value(1)), 30);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 4);
    EXPECT_EQ(std::get<std::int64_t>(*cursor.value(1)), 40);

    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(SSTableColumnCursorTest, SkipsLogicalBlocksWithoutMatchingRows) {
    TempFile file("htap_sstable_column_cursor_skips_non_matching_blocks.bin");

    ColumnFileBuilder builder;

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/0,
        /*min_key=*/1,
        /*max_key=*/3,
        /*values_count=*/2,
        make_key_block({1, 2})
    );

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/1,
        /*min_key=*/1,
        /*max_key=*/3,
        /*values_count=*/2,
        make_int64_column_block({10, 20})
    );

    builder.add_block(
        /*block_id=*/1,
        /*column_idx=*/0,
        /*min_key=*/3,
        /*max_key=*/5,
        /*values_count=*/2,
        make_key_block({3, 4})
    );

    builder.add_block(
        /*block_id=*/1,
        /*column_idx=*/1,
        /*min_key=*/3,
        /*max_key=*/5,
        /*values_count=*/2,
        make_int64_column_block({30, 40})
    );

    file.write(builder.file_data);

    SSTableColumnCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(3, 5),
        test_schema(),
        {0, 1}
    );

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 3);
    EXPECT_EQ(std::get<std::int64_t>(*cursor.value(1)), 30);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 4);
    EXPECT_EQ(std::get<std::int64_t>(*cursor.value(1)), 40);

    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(SSTableColumnCursorTest, DoesNotRequireUnprojectedColumnMetadata) {
    TempFile file("htap_sstable_column_cursor_missing_unprojected_metadata.bin");

    ColumnFileBuilder builder;

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/0,
        /*min_key=*/1,
        /*max_key=*/4,
        /*values_count=*/3,
        make_key_block({1, 2, 3})
    );

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/3,
        /*min_key=*/1,
        /*max_key=*/4,
        /*values_count=*/3,
        make_string_column_block({"a", "bb", "ccc"})
    );

    // column 1 and column 2 metadata are intentionally absent.
    file.write(builder.file_data);

    SSTableColumnCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(1, 4),
        test_schema(),
        {0, 3}
    );

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 1);
    EXPECT_EQ(std::get<std::string>(*cursor.value(3)), "a");

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 2);
    EXPECT_EQ(std::get<std::string>(*cursor.value(3)), "bb");

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 3);
    EXPECT_EQ(std::get<std::string>(*cursor.value(3)), "ccc");

    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(SSTableColumnCursorTest, ThrowsWhenAccessingColumnThatWasNotLoaded) {
    TempFile file("htap_sstable_column_cursor_unloaded_column.bin");

    ColumnFileBuilder builder;

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/0,
        /*min_key=*/1,
        /*max_key=*/3,
        /*values_count=*/2,
        make_key_block({1, 2})
    );

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/1,
        /*min_key=*/1,
        /*max_key=*/3,
        /*values_count=*/2,
        make_int64_column_block({10, 20})
    );

    file.write(builder.file_data);

    SSTableColumnCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(1, 3),
        test_schema(),
        {0, 1}
    );

    ASSERT_TRUE(cursor.valid());

    EXPECT_THROW(
        cursor.value(2),
        std::logic_error
    );
}

TEST(SSTableColumnCursorTest, ThrowsOnInvalidColumnIndex) {
    TempFile file("htap_sstable_column_cursor_invalid_column_index.bin");

    ColumnFileBuilder builder;

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/0,
        /*min_key=*/1,
        /*max_key=*/2,
        /*values_count=*/1,
        make_key_block({1})
    );

    file.write(builder.file_data);

    SSTableColumnCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(1, 2),
        test_schema(),
        {0}
    );

    ASSERT_TRUE(cursor.valid());

    EXPECT_THROW(
        cursor.value(100),
        std::out_of_range
    );
}

TEST(SSTableColumnCursorTest, ThrowsWhenKeyCalledOnInvalidCursor) {
    TempFile file("htap_sstable_column_cursor_key_on_invalid.bin");
    file.touch();

    SSTableColumnCursor cursor(
        file.path(),
        {},
        range(std::nullopt, std::nullopt),
        test_schema(),
        {0}
    );

    ASSERT_FALSE(cursor.valid());

    EXPECT_THROW(
        cursor.key(),
        std::logic_error
    );
}

TEST(SSTableColumnCursorTest, ThrowsWhenValueCalledOnInvalidCursor) {
    TempFile file("htap_sstable_column_cursor_value_on_invalid.bin");
    file.touch();

    SSTableColumnCursor cursor(
        file.path(),
        {},
        range(std::nullopt, std::nullopt),
        test_schema(),
        {0}
    );

    ASSERT_FALSE(cursor.valid());

    EXPECT_THROW(
        cursor.value(0),
        std::logic_error
    );
}

TEST(SSTableColumnCursorTest, ThrowsOnTruncatedKeyBlock) {
    TempFile file("htap_sstable_column_cursor_truncated_key_block.bin");

    std::vector<char> file_data;
    append_pod<std::int32_t>(file_data, 42); // less than sizeof(Key)

    std::vector<read::sstable::ColumnBlockMeta> blocks = {
        make_meta(
            /*min_key=*/1,
            /*max_key=*/2,
            /*offset=*/0,
            /*size=*/file_data.size(),
            /*values_count=*/1,
            /*block_id=*/0,
            /*column_idx=*/0
        )
    };

    file.write(file_data);

    EXPECT_THROW(
        SSTableColumnCursor(
            file.path(),
            std::move(blocks),
            range(1, 2),
            test_schema(),
            {0}
        ),
        std::runtime_error
    );
}

TEST(SSTableColumnCursorTest, ThrowsOnTruncatedValueBlock) {
    TempFile file("htap_sstable_column_cursor_truncated_value_block.bin");

    ColumnFileBuilder builder;

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/0,
        /*min_key=*/1,
        /*max_key=*/2,
        /*values_count=*/1,
        make_key_block({1})
    );

    // values_count = 1, bitmap says value is present, but no int64 bytes follow.
    std::vector<char> broken_value_block;
    broken_value_block.push_back(0);

    builder.add_block(
        /*block_id=*/0,
        /*column_idx=*/1,
        /*min_key=*/1,
        /*max_key=*/2,
        /*values_count=*/1,
        broken_value_block
    );

    file.write(builder.file_data);

    EXPECT_THROW(
        SSTableColumnCursor(
            file.path(),
            std::move(builder.blocks),
            range(1, 2),
            test_schema(),
            {0, 1}
        ),
        std::runtime_error
    );
}

} 
