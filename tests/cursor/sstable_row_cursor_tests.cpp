#include "storage/cursor/sstable_row_cursor.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace htap::storage::cursor {

namespace {

class TempFile {
public:
    explicit TempFile(std::string name)
        : path_(std::filesystem::temp_directory_path() / std::move(name)) {
        cleanup();
    }

    ~TempFile() {
        cleanup();
    }

    const std::filesystem::path& path() const {
        return path_;
    }

    void write(const std::vector<char>& data) const {
        std::ofstream out(data_path(), std::ios::binary);
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }

    void touch() const {
        std::ofstream out(data_path(), std::ios::binary);
    }

private:
    std::filesystem::path data_path() const {
        auto path = path_;
        path.replace_extension(".sst");
        return path;
    }

    std::filesystem::path index_path() const {
        auto path = path_;
        path.replace_extension(".idx");
        return path;
    }

    std::filesystem::path metadata_path() const {
        auto path = path_;
        path.replace_extension(".meta");
        return path;
    }

    void cleanup() const {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
        std::filesystem::remove(data_path(), ec);
        std::filesystem::remove(index_path(), ec);
        std::filesystem::remove(metadata_path(), ec);
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

read::sstable::RowBlockMeta make_meta(
    Key min_key,
    Key max_key,
    std::uint64_t offset,
    std::uint64_t size,
    std::uint32_t row_count,
    std::size_t block_id
) {
    read::sstable::RowBlockMeta meta{};
    meta.min_key = min_key;
    meta.max_key = max_key;
    meta.offset = offset;
    meta.size = size;
    meta.row_count = row_count;
    meta.block_id = block_id;

    return meta;
}

std::vector<ValueType> test_schema() {
    return {
        ValueType::INT64,  // 0 key
        ValueType::INT64,  // 1 age
        ValueType::DOUBLE, // 2 score
        ValueType::STRING  // 3 name
    };
}

read::sstable::KeyRange range(std::optional<Key> from, std::optional<Key> to) {
    read::sstable::KeyRange result;
    result.from = from;
    result.to = to;
    return result;
}

struct TestRow {
    Key key;
    std::optional<std::int64_t> age;
    std::optional<double> score;
    std::optional<std::string> name;
};

std::vector<char> make_row_block(const std::vector<TestRow>& rows) {
    std::vector<char> data;

    for (const auto& row : rows) {
        append_pod(data, row.key);

        // bitmap is over value columns only:
        // bit 0 -> age
        // bit 1 -> score
        // bit 2 -> name
        std::uint8_t bitmap = 0;

        if (!row.age.has_value()) {
            bitmap |= static_cast<std::uint8_t>(1u << 0);
        }

        if (!row.score.has_value()) {
            bitmap |= static_cast<std::uint8_t>(1u << 1);
        }

        if (!row.name.has_value()) {
            bitmap |= static_cast<std::uint8_t>(1u << 2);
        }

        append_pod(data, bitmap);

        if (row.age.has_value()) {
            append_pod(data, *row.age);
        }

        if (row.score.has_value()) {
            append_pod(data, *row.score);
        }

        if (row.name.has_value()) {
            append_string(data, *row.name);
        }
    }

    return data;
}

struct RowFileBuilder {
    std::vector<char> file_data;
    std::vector<read::sstable::RowBlockMeta> blocks;

    void add_block(
        Key min_key,
        Key max_key,
        std::size_t block_id,
        const std::vector<TestRow>& rows
    ) {
        const auto block_data = make_row_block(rows);
        const auto offset = static_cast<std::uint64_t>(file_data.size());

        file_data.insert(file_data.end(), block_data.begin(), block_data.end());

        blocks.push_back(make_meta(
            min_key,
            max_key,
            offset,
            static_cast<std::uint64_t>(block_data.size()),
            static_cast<std::uint32_t>(rows.size()),
            block_id
        ));
    }
};

} // namespace

TEST(SSTableRowCursorTest, EmptyBlocksProduceInvalidCursor) {
    TempFile file("htap_sstable_row_cursor_empty_blocks.bin");
    file.touch();

    SSTableRowCursor cursor(
        file.path(),
        {},
        range(std::nullopt, std::nullopt),
        test_schema(),
        {0, 1, 2, 3}
    );

    EXPECT_FALSE(cursor.valid());
}

TEST(SSTableRowCursorTest, ReadsRowsAndNullsInRange) {
    TempFile file("htap_sstable_row_cursor_reads_rows_and_nulls.bin");

    RowFileBuilder builder;
    builder.add_block(
        /*min_key=*/1,
        /*max_key=*/4,
        /*block_id=*/0,
        {
            TestRow{1, 10, 1.5, "a"},
            TestRow{2, std::nullopt, 2.5, "bb"},
            TestRow{3, 30, std::nullopt, "ccc"},
        }
    );

    file.write(builder.file_data);

    SSTableRowCursor cursor(
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

TEST(SSTableRowCursorTest, RespectsUpperExclusiveRangeBoundary) {
    TempFile file("htap_sstable_row_cursor_upper_exclusive.bin");

    RowFileBuilder builder;
    builder.add_block(
        /*min_key=*/1,
        /*max_key=*/5,
        /*block_id=*/0,
        {
            TestRow{1, 10, 1.0, "a"},
            TestRow{2, 20, 2.0, "b"},
            TestRow{3, 30, 3.0, "c"},
            TestRow{4, 40, 4.0, "d"},
        }
    );

    file.write(builder.file_data);

    SSTableRowCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(2, 4),
        test_schema(),
        {0, 1, 2, 3}
    );

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 2);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 3);

    cursor.next();
    EXPECT_FALSE(cursor.valid());
}

TEST(SSTableRowCursorTest, SupportsOpenEndedRanges) {
    TempFile file("htap_sstable_row_cursor_open_ended_ranges.bin");

    RowFileBuilder builder;
    builder.add_block(
        /*min_key=*/1,
        /*max_key=*/5,
        /*block_id=*/0,
        {
            TestRow{1, 10, 1.0, "a"},
            TestRow{2, 20, 2.0, "b"},
            TestRow{3, 30, 3.0, "c"},
            TestRow{4, 40, 4.0, "d"},
        }
    );

    file.write(builder.file_data);

    {
        SSTableRowCursor cursor(
            file.path(),
            builder.blocks,
            range(std::nullopt, 3),
            test_schema(),
            {0, 1, 2, 3}
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
        SSTableRowCursor cursor(
            file.path(),
            builder.blocks,
            range(3, std::nullopt),
            test_schema(),
            {0, 1, 2, 3}
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

TEST(SSTableRowCursorTest, SupportsMultipleBlocks) {
    TempFile file("htap_sstable_row_cursor_multiple_blocks.bin");

    RowFileBuilder builder;
    builder.add_block(
        /*min_key=*/1,
        /*max_key=*/3,
        /*block_id=*/0,
        {
            TestRow{1, 10, 1.0, "a"},
            TestRow{2, 20, 2.0, "b"},
        }
    );

    builder.add_block(
        /*min_key=*/3,
        /*max_key=*/5,
        /*block_id=*/1,
        {
            TestRow{3, 30, 3.0, "c"},
            TestRow{4, 40, 4.0, "d"},
        }
    );

    file.write(builder.file_data);

    SSTableRowCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(1, 5),
        test_schema(),
        {0, 1, 2, 3}
    );

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 1);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 2);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 3);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 4);

    cursor.next();
    EXPECT_FALSE(cursor.valid());
}

TEST(SSTableRowCursorTest, SkipsBlocksWithoutMatchingRows) {
    TempFile file("htap_sstable_row_cursor_skips_non_matching_blocks.bin");

    RowFileBuilder builder;
    builder.add_block(
        /*min_key=*/1,
        /*max_key=*/3,
        /*block_id=*/0,
        {
            TestRow{1, 10, 1.0, "a"},
            TestRow{2, 20, 2.0, "b"},
        }
    );

    builder.add_block(
        /*min_key=*/3,
        /*max_key=*/5,
        /*block_id=*/1,
        {
            TestRow{3, 30, 3.0, "c"},
            TestRow{4, 40, 4.0, "d"},
        }
    );

    file.write(builder.file_data);

    SSTableRowCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(3, 5),
        test_schema(),
        {0, 1, 2, 3}
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

TEST(SSTableRowCursorTest, ReturnsInvalidWhenRangeMatchesNoRows) {
    TempFile file("htap_sstable_row_cursor_range_matches_no_rows.bin");

    RowFileBuilder builder;
    builder.add_block(
        /*min_key=*/1,
        /*max_key=*/4,
        /*block_id=*/0,
        {
            TestRow{1, 10, 1.0, "a"},
            TestRow{2, 20, 2.0, "b"},
            TestRow{3, 30, 3.0, "c"},
        }
    );

    file.write(builder.file_data);

    SSTableRowCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(10, 20),
        test_schema(),
        {0, 1, 2, 3}
    );

    EXPECT_FALSE(cursor.valid());
}

TEST(SSTableRowCursorTest, ThrowsOnInvalidColumnIndex) {
    TempFile file("htap_sstable_row_cursor_invalid_column_index.bin");

    RowFileBuilder builder;
    builder.add_block(
        /*min_key=*/1,
        /*max_key=*/2,
        /*block_id=*/0,
        {
            TestRow{1, 10, 1.0, "a"},
        }
    );

    file.write(builder.file_data);

    SSTableRowCursor cursor(
        file.path(),
        std::move(builder.blocks),
        range(1, 2),
        test_schema(),
        {0, 1, 2, 3}
    );

    ASSERT_TRUE(cursor.valid());

    EXPECT_THROW(
        cursor.value(100),
        std::out_of_range
    );
}

TEST(SSTableRowCursorTest, ThrowsWhenKeyCalledOnInvalidCursor) {
    TempFile file("htap_sstable_row_cursor_key_on_invalid.bin");
    file.touch();

    SSTableRowCursor cursor(
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

TEST(SSTableRowCursorTest, ThrowsWhenValueCalledOnInvalidCursor) {
    TempFile file("htap_sstable_row_cursor_value_on_invalid.bin");
    file.touch();

    SSTableRowCursor cursor(
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

TEST(SSTableRowCursorTest, ThrowsOnTruncatedKey) {
    TempFile file("htap_sstable_row_cursor_truncated_key.bin");

    std::vector<char> file_data;
    append_pod(file_data, 42); // less than sizeof(Key)

    std::vector<read::sstable::RowBlockMeta> blocks = {
        make_meta(
            /*min_key=*/1,
            /*max_key=*/2,
            /*offset=*/0,
            /*size=*/file_data.size(),
            /*row_count=*/1,
            /*block_id=*/0
        )
    };

    file.write(file_data);

    EXPECT_THROW(
        SSTableRowCursor(
            file.path(),
            std::move(blocks),
            range(1, 2),
            test_schema(),
            {0, 1, 2, 3}
        ),
        std::runtime_error
    );
}

TEST(SSTableRowCursorTest, ThrowsOnTruncatedValue) {
    TempFile file("htap_sstable_row_cursor_truncated_value.bin");

    std::vector<char> file_data;

    const Key key = 1;
    const std::uint8_t bitmap = 0b00000000; // age, score, name are all present

    append_pod(file_data, key);
    append_pod(file_data, bitmap);

    // age is present but truncated: only int32 instead of int64
    append_pod(file_data, 10);

    std::vector<read::sstable::RowBlockMeta> blocks = {
        make_meta(
            /*min_key=*/1,
            /*max_key=*/2,
            /*offset=*/0,
            /*size=*/file_data.size(),
            /*row_count=*/1,
            /*block_id=*/0
        )
    };

    file.write(file_data);

    EXPECT_THROW(
        SSTableRowCursor(
            file.path(),
            std::move(blocks),
            range(1, 2),
            test_schema(),
            {0, 1, 2, 3}
        ),
        std::runtime_error
    );
}

} 
