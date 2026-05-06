#include "storage/cursor/chain_cursor.hpp"
#include "storage/cursor/empty_cursor.hpp"

#include <gtest/gtest.h>

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
using htap::storage::cursor::ChainCursor;
using htap::storage::cursor::EmptyCursor;

class VectorCursor final : public ICursor {
public:
    explicit VectorCursor(std::vector<std::pair<Key, Row>> rows)
        : rows_(std::move(rows)) {
    }

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

Row make_row(Key key, int64_t value) {
    return Row{
        NullableValue{Value{key}},
        NullableValue{Value{value}},
    };
}

std::unique_ptr<ICursor> make_vector_cursor(std::vector<std::pair<Key, Row>> rows) {
    return std::make_unique<VectorCursor>(std::move(rows));
}

TEST(ChainCursorTest, EmptyChainIsInvalid) {
    ChainCursor cursor({});

    EXPECT_FALSE(cursor.valid());

    cursor.next();

    EXPECT_FALSE(cursor.valid());

    EXPECT_THROW(
        {
            [[maybe_unused]] auto key = cursor.key();
        },
        std::logic_error
    );

    EXPECT_THROW(
        {
            [[maybe_unused]] auto value = cursor.value(0);
        },
        std::logic_error
    );
}

TEST(ChainCursorTest, SingleCursorWorks) {
    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(make_vector_cursor({
        {1, make_row(1, 10)},
        {2, make_row(2, 20)},
    }));

    ChainCursor cursor(std::move(cursors));

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 1);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 10);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 2);
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 20);

    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(ChainCursorTest, ReadsCursorsSequentiallyWithoutSorting) {
    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(make_vector_cursor({
        {10, make_row(10, 100)},
        {30, make_row(30, 300)},
    }));
    cursors.push_back(make_vector_cursor({
        {1, make_row(1, 10)},
        {2, make_row(2, 20)},
    }));

    ChainCursor cursor(std::move(cursors));

    std::vector<Key> keys;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({10, 30, 1, 2}));
}

TEST(ChainCursorTest, SkipsEmptyCursors) {
    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(std::make_unique<EmptyCursor>());
    cursors.push_back(make_vector_cursor({
        {5, make_row(5, 50)},
    }));
    cursors.push_back(std::make_unique<EmptyCursor>());
    cursors.push_back(make_vector_cursor({
        {6, make_row(6, 60)},
    }));
    cursors.push_back(std::make_unique<EmptyCursor>());

    ChainCursor cursor(std::move(cursors));

    std::vector<Key> keys;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({5, 6}));
}

TEST(ChainCursorTest, AllChildrenEmptyIsInvalid) {
    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(std::make_unique<EmptyCursor>());
    cursors.push_back(std::make_unique<EmptyCursor>());

    ChainCursor cursor(std::move(cursors));

    EXPECT_FALSE(cursor.valid());

    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(ChainCursorTest, ProxiesValueToCurrentCursor) {
    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(make_vector_cursor({
        {1, make_row(1, 10)},
    }));
    cursors.push_back(make_vector_cursor({
        {2, make_row(2, 20)},
    }));

    ChainCursor cursor(std::move(cursors));

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 10);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 20);
}

TEST(ChainCursorTest, NextOnInvalidCursorDoesNothing) {
    ChainCursor cursor({});

    EXPECT_FALSE(cursor.valid());

    cursor.next();
    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

}