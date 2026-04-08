#include <gtest/gtest.h>

#include "storage/model/row.hpp"

namespace htap::storage {
namespace {

TEST(RowTest, StoresAndRetrievesKey) {
    Row row(3);

    row.set_key(42);

    EXPECT_EQ(row.key(), 42);
}

TEST(RowTest, StoresAndRetrievesValues) {
    Row row(2);

    row.set_value(0, int64_t(10));
    row.set_value(1, std::string("hello"));

    EXPECT_TRUE(row.has_value(0));
    EXPECT_TRUE(row.has_value(1));

    auto v0 = row.get_value(0);
    ASSERT_TRUE(v0.has_value());
    EXPECT_EQ(std::get<int64_t>(*v0), 10);

    auto v1 = row.get_value(1);
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(std::get<std::string>(*v1), "hello");
}

TEST(RowTest, HandlesNullValues) {
    Row row(2);

    row.set_value(0, std::nullopt);

    EXPECT_FALSE(row.has_value(0));

    auto v = row.get_value(0);
    EXPECT_FALSE(v.has_value());
}

TEST(RowTest, InitializesWithColumnCount) {
    Row row(5);

    EXPECT_EQ(row.size(), 5u);

    for (size_t i = 0; i < row.size(); ++i) {
        EXPECT_FALSE(row.has_value(i));
    }
}

TEST(RowTest, OverwritesValues) {
    Row row(1);

    row.set_value(0, int64_t(1));
    row.set_value(0, int64_t(2));

    auto v = row.get_value(0);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(std::get<int64_t>(*v), 2);
}

} // namespace
} // namespace htap::storage
