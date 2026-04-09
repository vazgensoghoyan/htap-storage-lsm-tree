#include <gtest/gtest.h>

#include "storage/mock/mock_cursor.hpp"

using namespace htap::storage;

TEST(MockCursorTest, ValidIterationWorks) {
    std::vector<int64_t> keys = {1, 2};
    std::vector<std::vector<NullableValue>> rows = {
        {int64_t(10)},
        {int64_t(20)}
    };

    MockCursor cursor(keys, rows, {0});

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 1);

    cursor.next();
    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 2);

    cursor.next();
    EXPECT_FALSE(cursor.valid());
}

TEST(MockCursorTest, ValueAccessWorks) {
    std::vector<int64_t> keys = {1};
    std::vector<std::vector<NullableValue>> rows = {
        {int64_t(42)}
    };

    MockCursor cursor(keys, rows, {0});

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(std::get<int64_t>(cursor.value(0)), 42);
}

TEST(MockCursorTest, ProjectionWorks) {
    std::vector<int64_t> keys = {1};
    std::vector<std::vector<NullableValue>> rows = {
        {int64_t(10), std::string("abc")}
    };

    // берём только вторую колонку
    MockCursor cursor(keys, rows, {1});

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(std::get<std::string>(cursor.value(0)), "abc");
}

TEST(MockCursorTest, NullHandlingWorks) {
    std::vector<int64_t> keys = {1};
    std::vector<std::vector<NullableValue>> rows = {
        {std::nullopt}
    };

    MockCursor cursor(keys, rows, {0});

    ASSERT_TRUE(cursor.valid());
    EXPECT_TRUE(cursor.is_null(0));

    EXPECT_THROW(cursor.value(0), std::runtime_error);
}

TEST(MockCursorTest, ThrowsWhenInvalidCursor) {
    std::vector<int64_t> keys = {};
    std::vector<std::vector<NullableValue>> rows = {};

    MockCursor cursor(keys, rows, {0});

    EXPECT_FALSE(cursor.valid());

    EXPECT_THROW(cursor.key(), std::out_of_range);
    EXPECT_THROW(cursor.value(0), std::out_of_range);
    EXPECT_THROW(cursor.is_null(0), std::out_of_range);
}

TEST(MockCursorTest, ThrowsOnInvalidColumnIndex) {
    std::vector<int64_t> keys = {1};
    std::vector<std::vector<NullableValue>> rows = {
        {int64_t(10)}
    };

    MockCursor cursor(keys, rows, {0});

    ASSERT_TRUE(cursor.valid());

    EXPECT_THROW(cursor.value(1), std::out_of_range);
    EXPECT_THROW(cursor.is_null(1), std::out_of_range);
}
