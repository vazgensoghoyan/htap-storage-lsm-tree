#include <gtest/gtest.h>

#include "lsmtree/mem/imm_memtable.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

TEST(ImmutableMemTableTest, EmptyData) {
    std::vector<Row> data;

    ImmutableMemTable imm(std::move(data));

    EXPECT_EQ(imm.size(), 0);
}

TEST(ImmutableMemTableTest, SizeMatchesInput) {
    std::vector<Row> data;
    data.push_back({1, 100});
    data.push_back({2, 200});
    data.push_back({3, 300});

    ImmutableMemTable imm(std::move(data));

    EXPECT_EQ(imm.size(), 3);
}

TEST(ImmutableMemTableTest, MoveConstructorEmptiesSourceVector) {
    std::vector<Row> data;
    data.push_back({1, 100});
    data.push_back({2, 200});

    ImmutableMemTable imm(std::move(data));

    EXPECT_TRUE(data.empty() || data.size() == 0);
    EXPECT_EQ(imm.size(), 2);
}

TEST(ImmutableMemTableTest, LargeInput) {
    std::vector<Row> data;

    const int N = 10000;
    for (int i = 0; i < N; ++i) {
        data.push_back({i, i * 10});
    }

    ImmutableMemTable imm(std::move(data));

    EXPECT_EQ(imm.size(), N);
}

TEST(ImmutableMemTableTest, MultipleInstancesIndependent) {
    std::vector<Row> data1;
    data1.push_back({1, 100});

    std::vector<Row> data2;
    data2.push_back({2, 200});
    data2.push_back({3, 300});

    ImmutableMemTable imm1(std::move(data1));
    ImmutableMemTable imm2(std::move(data2));

    EXPECT_EQ(imm1.size(), 1);
    EXPECT_EQ(imm2.size(), 2);
}
