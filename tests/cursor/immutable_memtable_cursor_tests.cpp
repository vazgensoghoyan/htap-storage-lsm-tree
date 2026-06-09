#include "storage/cursor/immutable_memtable_cursor.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

using htap::storage::KEY_COLUMN_INDEX;
using htap::storage::Key;
using htap::storage::NullableValue;
using htap::storage::Row;
using htap::storage::Value;
using htap::storage::cursor::ImmutableMemTableCursor;

Row MakeRow(Key key, int64_t value) {
    return Row{
        NullableValue{Value{key}},
        NullableValue{Value{value}},
    };
}

TEST(ImmutableMemTableCursorTest, EmptyStorageIsInvalid) {
    ImmutableMemTableCursor::Storage storage;

    ImmutableMemTableCursor cursor(storage.begin(), storage.end());

    EXPECT_FALSE(cursor.valid());

    cursor.next();
    EXPECT_FALSE(cursor.valid());

    EXPECT_THROW(
        { [[maybe_unused]] auto key = cursor.key(); },
        std::logic_error
    );

    EXPECT_THROW(
        { [[maybe_unused]] auto value = cursor.value(0); },
        std::logic_error
    );
}

TEST(ImmutableMemTableCursorTest, IteratesOverFullStorage) {
    ImmutableMemTableCursor::Storage storage{
        MakeRow(1, 10),
        MakeRow(2, 20),
        MakeRow(3, 30),
    };

    ImmutableMemTableCursor cursor(storage.begin(), storage.end());

    std::vector<Key> keys;
    std::vector<int64_t> values;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        values.push_back(std::get<int64_t>(*cursor.value(1)));
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({1, 2, 3}));
    EXPECT_EQ(values, std::vector<int64_t>({10, 20, 30}));
}

TEST(ImmutableMemTableCursorTest, IteratesOverSelectedRange) {
    ImmutableMemTableCursor::Storage storage{
        MakeRow(1, 10),
        MakeRow(2, 20),
        MakeRow(3, 30),
        MakeRow(4, 40),
        MakeRow(5, 50),
    };

    auto begin = storage.begin() + 1;
    auto end = storage.begin() + 4;

    ImmutableMemTableCursor cursor(begin, end);

    std::vector<Key> keys;
    std::vector<int64_t> values;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        values.push_back(std::get<int64_t>(*cursor.value(1)));
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({2, 3, 4}));
    EXPECT_EQ(values, std::vector<int64_t>({20, 30, 40}));
}

TEST(ImmutableMemTableCursorTest, EndIteratorIsExclusive) {
    ImmutableMemTableCursor::Storage storage{
        MakeRow(1, 10),
        MakeRow(2, 20),
        MakeRow(3, 30),
    };

    ImmutableMemTableCursor cursor(storage.begin(), storage.begin() + 2);

    std::vector<Key> keys;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({1, 2}));
}

TEST(ImmutableMemTableCursorTest, EmptySelectedRangeIsInvalid) {
    ImmutableMemTableCursor::Storage storage{
        MakeRow(1, 10),
        MakeRow(2, 20),
        MakeRow(3, 30),
    };

    auto begin = storage.begin() + 2;
    auto end = storage.begin() + 2;

    ImmutableMemTableCursor cursor(begin, end);

    EXPECT_FALSE(cursor.valid());

    cursor.next();
    EXPECT_FALSE(cursor.valid());
}

TEST(ImmutableMemTableCursorTest, KeyIsReadFromKeyColumnIndex) {
    ImmutableMemTableCursor::Storage storage{
        Row{
            NullableValue{Value{42}},
            NullableValue{Value{100}},
            NullableValue{Value{200}},
        },
    };

    ImmutableMemTableCursor cursor(storage.begin(), storage.end());

    ASSERT_TRUE(cursor.valid());

    EXPECT_EQ(cursor.key(), 42);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(KEY_COLUMN_INDEX)), 42);
}

TEST(ImmutableMemTableCursorTest, ValueReturnsRequestedColumn) {
    ImmutableMemTableCursor::Storage storage{
        Row{
            NullableValue{Value{10}},
            NullableValue{Value{100}},
            NullableValue{Value{200}},
        },
    };

    ImmutableMemTableCursor cursor(storage.begin(), storage.end());

    ASSERT_TRUE(cursor.valid());

    EXPECT_EQ(cursor.key(), 10);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(0)), 10);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 100);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(2)), 200);
}

TEST(ImmutableMemTableCursorTest, ThrowsOnInvalidColumnIndex) {
    ImmutableMemTableCursor::Storage storage{
        MakeRow(1, 10),
    };

    ImmutableMemTableCursor cursor(storage.begin(), storage.end());

    ASSERT_TRUE(cursor.valid());

    EXPECT_THROW(
        { [[maybe_unused]] auto value = cursor.value(42); },
        std::out_of_range
    );
}

TEST(ImmutableMemTableCursorTest, NextOnInvalidCursorDoesNothing) {
    ImmutableMemTableCursor::Storage storage;

    ImmutableMemTableCursor cursor(storage.begin(), storage.end());

    EXPECT_FALSE(cursor.valid());

    cursor.next();
    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(ImmutableMemTableCursorTest, ThrowsAfterExhaustion) {
    ImmutableMemTableCursor::Storage storage{
        MakeRow(1, 10),
    };

    ImmutableMemTableCursor cursor(storage.begin(), storage.end());

    ASSERT_TRUE(cursor.valid());

    cursor.next();

    ASSERT_FALSE(cursor.valid());

    EXPECT_THROW(
        { [[maybe_unused]] auto key = cursor.key(); },
        std::logic_error
    );

    EXPECT_THROW(
        { [[maybe_unused]] auto value = cursor.value(0); },
        std::logic_error
    );
}

TEST(ImmutableMemTableCursorTest, SupportsNullValues) {
    ImmutableMemTableCursor::Storage storage{
        Row{
            NullableValue{Value{1}},
            NullableValue{},
        },
    };

    ImmutableMemTableCursor cursor(storage.begin(), storage.end());

    ASSERT_TRUE(cursor.valid());

    EXPECT_EQ(cursor.key(), 1);
    EXPECT_TRUE(cursor.value(0).has_value());
    EXPECT_FALSE(cursor.value(1).has_value());
}

}
