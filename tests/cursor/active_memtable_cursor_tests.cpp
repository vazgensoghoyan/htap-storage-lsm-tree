#include "storage/cursor/active_memtable_cursor.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

namespace {

using htap::storage::Key;
using htap::storage::NullableValue;
using htap::storage::Row;
using htap::storage::Value;
using htap::storage::cursor::ActiveMemTableCursor;

Row MakeRow(Key key, int64_t value) {
    return Row{
        NullableValue{Value{key}},
        NullableValue{Value{value}},
    };
}

TEST(ActiveMemTableCursorTest, EmptyStorageIsInvalid) {
    ActiveMemTableCursor::Storage storage;

    ActiveMemTableCursor cursor(storage.begin(), storage.end());

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

TEST(ActiveMemTableCursorTest, IteratesOverFullStorage) {
    ActiveMemTableCursor::Storage storage;

    storage.emplace(1, MakeRow(1, 10));
    storage.emplace(2, MakeRow(2, 20));
    storage.emplace(3, MakeRow(3, 30));

    ActiveMemTableCursor cursor(storage.begin(), storage.end());

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

TEST(ActiveMemTableCursorTest, IteratesOverSelectedRange) {
    ActiveMemTableCursor::Storage storage;

    storage.emplace(1, MakeRow(1, 10));
    storage.emplace(2, MakeRow(2, 20));
    storage.emplace(3, MakeRow(3, 30));
    storage.emplace(4, MakeRow(4, 40));
    storage.emplace(5, MakeRow(5, 50));

    auto begin = storage.lower_bound(2);
    auto end = storage.upper_bound(4);

    ActiveMemTableCursor cursor(begin, end);

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

TEST(ActiveMemTableCursorTest, EmptySelectedRangeIsInvalid) {
    ActiveMemTableCursor::Storage storage;

    storage.emplace(1, MakeRow(1, 10));
    storage.emplace(2, MakeRow(2, 20));
    storage.emplace(3, MakeRow(3, 30));

    auto begin = storage.lower_bound(10);
    auto end = storage.end();

    ActiveMemTableCursor cursor(begin, end);

    EXPECT_FALSE(cursor.valid());
}

TEST(ActiveMemTableCursorTest, ValueReturnsRequestedColumn) {
    ActiveMemTableCursor::Storage storage;

    storage.emplace(10, Row{
        NullableValue{Value{10}},
        NullableValue{Value{100}},
        NullableValue{Value{200}},
    });

    ActiveMemTableCursor cursor(storage.begin(), storage.end());

    ASSERT_TRUE(cursor.valid());

    EXPECT_EQ(cursor.key(), 10);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(0)), 10);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 100);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(2)), 200);
}

TEST(ActiveMemTableCursorTest, ThrowsOnInvalidColumnIndex) {
    ActiveMemTableCursor::Storage storage;

    storage.emplace(1, MakeRow(1, 10));

    ActiveMemTableCursor cursor(storage.begin(), storage.end());

    ASSERT_TRUE(cursor.valid());

    EXPECT_THROW(
        { [[maybe_unused]] auto value = cursor.value(42); },
        std::out_of_range
    );
}

TEST(ActiveMemTableCursorTest, NextOnInvalidCursorDoesNothing) {
    ActiveMemTableCursor::Storage storage;

    ActiveMemTableCursor cursor(storage.begin(), storage.end());

    EXPECT_FALSE(cursor.valid());

    cursor.next();
    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(ActiveMemTableCursorTest, ThrowsAfterExhaustion) {
    ActiveMemTableCursor::Storage storage;

    storage.emplace(1, MakeRow(1, 10));

    ActiveMemTableCursor cursor(storage.begin(), storage.end());

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

TEST(ActiveMemTableCursorTest, SupportsNullValues) {
    ActiveMemTableCursor::Storage storage;

    storage.emplace(1, Row{
        NullableValue{Value{1}},
        NullableValue{},
    });

    ActiveMemTableCursor cursor(storage.begin(), storage.end());

    ASSERT_TRUE(cursor.valid());

    EXPECT_EQ(cursor.key(), 1);
    EXPECT_TRUE(cursor.value(0).has_value());
    EXPECT_FALSE(cursor.value(1).has_value());
}

TEST(ActiveMemTableCursorTest, EndIteratorIsExclusive) {
    ActiveMemTableCursor::Storage storage;

    storage.emplace(1, MakeRow(1, 10));
    storage.emplace(2, MakeRow(2, 20));
    storage.emplace(3, MakeRow(3, 30));

    auto begin = storage.lower_bound(1);
    auto end = storage.lower_bound(3);

    ActiveMemTableCursor cursor(begin, end);

    std::vector<Key> keys;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({1, 2}));
}

} 