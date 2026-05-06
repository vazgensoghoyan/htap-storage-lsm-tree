#include "storage/cursor/chain_cursor.hpp"
#include "storage/cursor/empty_cursor.hpp"
#include "storage/mock/mock_cursor.hpp"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using htap::storage::ICursor;
using htap::storage::Key;
using htap::storage::NullableValue;
using htap::storage::Row;
using htap::storage::Value;
using htap::storage::MockCursor;
using htap::storage::cursor::ChainCursor;
using htap::storage::cursor::EmptyCursor;

Row make_row(Key key, int64_t value) {
    return Row{
        NullableValue{Value{key}},
        NullableValue{Value{value}},
    };
}

std::unique_ptr<ICursor> make_mock_cursor(
    std::vector<Key> keys,
    const std::map<Key, Row>* data,
    std::vector<std::size_t> projection = {0, 1}
) {
    return std::make_unique<MockCursor>(
        std::move(keys),
        data,
        projection
    );
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
    std::map<Key, Row> data{
        {1, make_row(1, 10)},
        {2, make_row(2, 20)},
    };

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(make_mock_cursor({1, 2}, &data));

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
    std::map<Key, Row> data{
        {1, make_row(1, 10)},
        {2, make_row(2, 20)},
        {10, make_row(10, 100)},
        {30, make_row(30, 300)},
    };

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(make_mock_cursor({10, 30}, &data));
    cursors.push_back(make_mock_cursor({1, 2}, &data));

    ChainCursor cursor(std::move(cursors));

    std::vector<Key> keys;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({10, 30, 1, 2}));
}

TEST(ChainCursorTest, SkipsEmptyCursors) {
    std::map<Key, Row> data{
        {5, make_row(5, 50)},
        {6, make_row(6, 60)},
    };

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(std::make_unique<EmptyCursor>());
    cursors.push_back(make_mock_cursor({5}, &data));
    cursors.push_back(std::make_unique<EmptyCursor>());
    cursors.push_back(make_mock_cursor({6}, &data));
    cursors.push_back(std::make_unique<EmptyCursor>());

    ChainCursor cursor(std::move(cursors));

    std::vector<Key> keys;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }

    EXPECT_EQ(keys, std::vector<Key>({5, 6}));
}

TEST(ChainCursorTest, ProxiesValueToCurrentCursor) {
    std::map<Key, Row> data{
        {1, make_row(1, 10)},
        {2, make_row(2, 20)},
    };

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(make_mock_cursor({1}, &data));
    cursors.push_back(make_mock_cursor({2}, &data));

    ChainCursor cursor(std::move(cursors));

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 10);

    cursor.next();

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(std::get<int64_t>(*cursor.value(1)), 20);
}

TEST(ChainCursorTest, ValueThrowsIfCurrentCursorThrows) {
    std::map<Key, Row> data{
        {1, make_row(1, 10)},
    };

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(make_mock_cursor({1}, &data, {0}));

    ChainCursor cursor(std::move(cursors));

    ASSERT_TRUE(cursor.valid());

    EXPECT_THROW(
        {
            [[maybe_unused]] auto value = cursor.value(1);
        },
        std::runtime_error
    );
}

TEST(ChainCursorTest, NextOnInvalidCursorDoesNothing) {
    ChainCursor cursor({});

    EXPECT_FALSE(cursor.valid());

    cursor.next();
    cursor.next();

    EXPECT_FALSE(cursor.valid());
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

} 