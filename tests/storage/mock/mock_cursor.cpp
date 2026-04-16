#include <gtest/gtest.h>

#include "storage/mock/mock_cursor.hpp"

using namespace htap::storage;

class MockCursorTest : public ::testing::Test {
protected:
    using Storage = std::map<Key, Row>;

    void SetUp() override {
        data = {
            {1, {
                Value(int64_t(10)),
                Value(std::string("a"))
            }},
            {2, {
                std::nullopt,
                Value(std::string("b"))
            }},
            {3, {
                Value(int64_t(30)),
                std::nullopt
            }},
            {5, {
                Value(int64_t(50)),
                Value(std::string("e"))
            }},
        };
    }

    Storage data;
};

TEST_F(MockCursorTest, BasicIteration) {
    MockCursor cursor(data, {0, 1}, 1, 6);

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 1);

    cursor.next();
    EXPECT_EQ(cursor.key(), 2);

    cursor.next();
    EXPECT_EQ(cursor.key(), 3);

    cursor.next();
    EXPECT_EQ(cursor.key(), 5);

    cursor.next();
    EXPECT_FALSE(cursor.valid());
}

TEST_F(MockCursorTest, RangeBounds) {
    MockCursor cursor(data, {0, 1}, 2, 5);

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 2);

    cursor.next();
    EXPECT_EQ(cursor.key(), 3);

    cursor.next();
    EXPECT_FALSE(cursor.valid()); // 5 excluded
}

TEST_F(MockCursorTest, EmptyRange) {
    MockCursor cursor(data, {0, 1}, 10, 20);

    EXPECT_FALSE(cursor.valid());
}

TEST_F(MockCursorTest, SeekInsideRange) {
    MockCursor cursor(data, {0, 1}, 1, 6);

    cursor.seek(3);

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 3);
}

TEST_F(MockCursorTest, SeekNonExistingKey) {
    MockCursor cursor(data, {0, 1}, 1, 6);

    cursor.seek(4); // → 5

    ASSERT_TRUE(cursor.valid());
    EXPECT_EQ(cursor.key(), 5);
}

TEST_F(MockCursorTest, SeekOutsideRange) {
    MockCursor cursor(data, {0, 1}, 1, 5);

    cursor.seek(100);

    EXPECT_FALSE(cursor.valid());
}

TEST_F(MockCursorTest, ValueAndNulls) {
    MockCursor cursor(data, {0, 1}, 1, 2);

    ASSERT_TRUE(cursor.valid());

    EXPECT_FALSE(cursor.is_null(0));
    EXPECT_EQ(std::get<int64_t>(cursor.value(0)), 10);

    EXPECT_FALSE(cursor.is_null(1));
    EXPECT_EQ(std::get<std::string>(cursor.value(1)), "a");
}

TEST_F(MockCursorTest, NullHandling) {
    MockCursor cursor(data, {0, 1}, 2, 3);

    ASSERT_TRUE(cursor.valid());

    EXPECT_TRUE(cursor.is_null(0));
    EXPECT_FALSE(cursor.is_null(1));
}

TEST_F(MockCursorTest, ValueThrowsOnNull) {
    MockCursor cursor(data, {0, 1}, 2, 3);

    ASSERT_TRUE(cursor.valid());

    EXPECT_THROW(cursor.value(0), std::runtime_error);
}

TEST_F(MockCursorTest, ProjectionRestriction) {
    MockCursor cursor(data, {1}, 1, 2);

    ASSERT_TRUE(cursor.valid());

    EXPECT_NO_THROW(cursor.is_null(1));

    EXPECT_THROW(cursor.is_null(0), std::runtime_error);
    EXPECT_THROW(cursor.value(0), std::runtime_error);
}

TEST_F(MockCursorTest, InvalidColumnIndex) {
    MockCursor cursor(data, {0, 1}, 1, 2);

    ASSERT_TRUE(cursor.valid());

    EXPECT_THROW(cursor.is_null(10), std::runtime_error);
    EXPECT_THROW(cursor.value(10), std::runtime_error);
}

TEST_F(MockCursorTest, AccessOnInvalidCursor) {
    MockCursor cursor(data, {0, 1}, 10, 20);

    EXPECT_FALSE(cursor.valid());

    EXPECT_THROW(cursor.key(), std::runtime_error);
    EXPECT_THROW(cursor.is_null(0), std::runtime_error);
    EXPECT_THROW(cursor.value(0), std::runtime_error);
}

TEST_F(MockCursorTest, MultipleSeeks) {
    MockCursor cursor(data, {0, 1}, 1, 6);

    cursor.seek(2);
    EXPECT_EQ(cursor.key(), 2);

    cursor.seek(5);
    EXPECT_EQ(cursor.key(), 5);

    cursor.seek(100);
    EXPECT_FALSE(cursor.valid());
}

TEST_F(MockCursorTest, NextAfterEndIsSafe) {
    MockCursor cursor(data, {0, 1}, 1, 2);

    ASSERT_TRUE(cursor.valid());

    cursor.next();
    EXPECT_FALSE(cursor.valid());

    EXPECT_NO_THROW(cursor.next());
}
