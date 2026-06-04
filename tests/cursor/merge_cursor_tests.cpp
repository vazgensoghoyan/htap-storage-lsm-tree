#include "storage/cursor/merge_cursor.hpp"
#include "storage/cursor/empty_cursor.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using htap::storage::ICursor;
using htap::storage::Key;
using htap::storage::NullableValue;
using htap::storage::Row;
using htap::storage::Value;
using htap::storage::cursor::EmptyCursor;
using htap::storage::cursor::MergeCursor;

class VectorCursor final : public ICursor {
public:
    explicit VectorCursor(std::vector<std::pair<Key, Row>> rows)
        : rows_(std::move(rows))
    {}

    bool valid() const override {
        return pos_ < rows_.size();
    }

    void next() override {
        if (valid()) {
            ++pos_;
        }
    }

    Key key() const override {
        if (!valid()) {
            throw std::logic_error("VectorCursor::key() called on invalid cursor");
        }

        return rows_[pos_].first;
    }

    NullableValue value(std::size_t column_idx) const override {
        if (!valid()) {
            throw std::logic_error("VectorCursor::value() called on invalid cursor");
        }

        const Row& row = rows_[pos_].second;

        if (column_idx >= row.size()) {
            throw std::out_of_range("Column index out of range");
        }

        return row[column_idx];
    }

private:
    std::vector<std::pair<Key, Row>> rows_;
    std::size_t pos_ = 0;
};

Row MakeRow(Key key, int64_t value) {
    return Row{
        NullableValue{Value{key}},
        NullableValue{Value{value}},
    };
}

std::unique_ptr<ICursor> MakeVectorCursor(
    std::vector<std::pair<Key, Row>> rows
) {
    return std::make_unique<VectorCursor>(std::move(rows));
}

TEST(MergeCursorTest, EmptyMergeIsInvalid) {
    MergeCursor cursor({});

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

TEST(MergeCursorTest, SingleCursorWorks) {
    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(MakeVectorCursor({
        {1, MakeRow(1, 10)},
        {3, MakeRow(3, 30)},
        {5, MakeRow(5, 50)},
    }));

    MergeCursor cursor(std::move(cursors));

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 1);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 10);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 3);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 30);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 5);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 50);

    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(MergeCursorTest, MergesTwoSortedCursors) {
    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(MakeVectorCursor({
        {1, MakeRow(1, 10)},
        {4, MakeRow(4, 40)},
    }));

    cursors.push_back(MakeVectorCursor({
        {2, MakeRow(2, 20)},
        {3, MakeRow(3, 30)},
        {5, MakeRow(5, 50)},
    }));

    MergeCursor cursor(std::move(cursors));

    std::vector<Key> keys;
    std::vector<int64_t> values;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        values.push_back(std::get<int64_t>(*cursor.value(1)));
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({1, 2, 3, 4, 5}));
    EXPECT_EQ(values, std::vector<int64_t>({10, 20, 30, 40, 50}));
}

TEST(MergeCursorTest, MergesManySortedCursors) {
    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(MakeVectorCursor({
        {1, MakeRow(1, 10)},
        {7, MakeRow(7, 70)},
    }));

    cursors.push_back(MakeVectorCursor({
        {2, MakeRow(2, 20)},
        {5, MakeRow(5, 50)},
    }));

    cursors.push_back(MakeVectorCursor({
        {3, MakeRow(3, 30)},
        {4, MakeRow(4, 40)},
        {6, MakeRow(6, 60)},
    }));

    MergeCursor cursor(std::move(cursors));

    std::vector<Key> keys;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({1, 2, 3, 4, 5, 6, 7}));
}

TEST(MergeCursorTest, SkipsEmptyChildCursors) {
    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(std::make_unique<EmptyCursor>());

    cursors.push_back(MakeVectorCursor({
        {2, MakeRow(2, 20)},
    }));

    cursors.push_back(std::make_unique<EmptyCursor>());

    cursors.push_back(MakeVectorCursor({
        {1, MakeRow(1, 10)},
        {3, MakeRow(3, 30)},
    }));

    MergeCursor cursor(std::move(cursors));

    std::vector<Key> keys;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({1, 2, 3}));
}

TEST(MergeCursorTest, ValueIsTakenFromCurrentMinKeyCursor) {
    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(MakeVectorCursor({
        {10, MakeRow(10, 1000)},
    }));

    cursors.push_back(MakeVectorCursor({
        {1, MakeRow(1, 100)},
        {20, MakeRow(20, 2000)},
    }));

    MergeCursor cursor(std::move(cursors));

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 1);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 100);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 10);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 1000);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 20);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 2000);
}

TEST(MergeCursorTest, DuplicateKeysAreReturnedDeterministically) {
    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(MakeVectorCursor({
        {1, MakeRow(1, 100)},
    }));

    cursors.push_back(MakeVectorCursor({
        {1, MakeRow(1, 200)},
    }));

    MergeCursor cursor(std::move(cursors));

    std::vector<Key> keys;
    std::vector<int64_t> values;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        values.push_back(std::get<int64_t>(*cursor.value(1)));
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({1, 1}));
    EXPECT_EQ(values, std::vector<int64_t>({100, 200}));
}

TEST(MergeCursorTest, NextOnInvalidCursorDoesNothing) {
    MergeCursor cursor({});

    EXPECT_FALSE(cursor.valid());

    cursor.next();
    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(MergeCursorTest, ThrowsWhenReadingInvalidCursorAfterExhaustion) {
    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(MakeVectorCursor({
        {1, MakeRow(1, 10)},
    }));

    MergeCursor cursor(std::move(cursors));

    ASSERT_TRUE(cursor.valid());
    cursor.next();

    ASSERT_FALSE(cursor.valid());

    EXPECT_THROW(
        { [[maybe_unused]] auto key = cursor.key(); },
        std::logic_error
    );

    EXPECT_THROW(
        { [[maybe_unused]] auto value = cursor.value(1); },
        std::logic_error
    );
}

TEST(MergeCursorTest, ThrowsOnInvalidColumnIndex) {
    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(MakeVectorCursor({
        {1, MakeRow(1, 10)},
    }));

    MergeCursor cursor(std::move(cursors));

    ASSERT_TRUE(cursor.valid());

    EXPECT_THROW(
        { [[maybe_unused]] auto value = cursor.value(42); },
        std::out_of_range
    );
}

} 
