#include "storage/cursor/empty_cursor.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

using htap::storage::cursor::EmptyCursor;

TEST(EmptyCursorTest, ValidIsFalse) {
    EmptyCursor cursor;

    EXPECT_FALSE(cursor.valid());
}

TEST(EmptyCursorTest, NextDoesNothing) {
    EmptyCursor cursor;

    cursor.next();

    EXPECT_FALSE(cursor.valid());

    cursor.next();

    EXPECT_FALSE(cursor.valid());
}

TEST(EmptyCursorTest, KeyThrowsError) {
    EmptyCursor cursor;

    EXPECT_THROW(
        {
            [[maybe_unused]] auto key = cursor.key();
        },
        std::logic_error
    );
}

TEST(EmptyCursorTest, ValueThrowsError) {
    EmptyCursor cursor;

    EXPECT_THROW(
        {
            [[maybe_unused]] auto value = cursor.value(17);
        },
        std::logic_error
    );
}

}
