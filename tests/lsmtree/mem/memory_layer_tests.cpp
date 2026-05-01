#include <gtest/gtest.h>

#include "lsmtree/mem/memory_layer.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

TEST(MemoryLayerTest, InitiallyEmpty) {
    MemoryLayer layer(10);

    EXPECT_EQ(layer.immutable_count(), 0);
}

TEST(MemoryLayerTest, NoFreezeBelowThreshold) {
    MemoryLayer layer(3);

    layer.insert(1, {1, 100});
    layer.insert(2, {2, 200});

    EXPECT_EQ(layer.immutable_count(), 0);
}

TEST(MemoryLayerTest, FreezeOnThreshold) {
    MemoryLayer layer(3);

    layer.insert(1, {1, 100});
    layer.insert(2, {2, 200});
    layer.insert(3, {3, 300}); // threshold reached

    EXPECT_EQ(layer.immutable_count(), 1);
}

TEST(MemoryLayerTest, MultipleFreezes) {
    MemoryLayer layer(2);

    layer.insert(1, {1, 100});
    layer.insert(2, {2, 200}); // freeze #1

    layer.insert(3, {3, 300});
    layer.insert(4, {4, 400}); // freeze #2

    EXPECT_EQ(layer.immutable_count(), 2);
}

TEST(MemoryLayerTest, ForceFreezeCreatesImmutable) {
    MemoryLayer layer(100);

    layer.insert(1, {1, 100});
    layer.insert(2, {2, 200});

    layer.force_freeze();

    EXPECT_EQ(layer.immutable_count(), 1);
}

TEST(MemoryLayerTest, ForceFreezeOnEmpty) {
    MemoryLayer layer(100);

    layer.force_freeze();

    EXPECT_EQ(layer.immutable_count(), 1);
}

TEST(MemoryLayerTest, InsertAfterFreezeContinuesWorking) {
    MemoryLayer layer(2);

    layer.insert(1, {1, 100});
    layer.insert(2, {2, 200}); // freeze

    layer.insert(3, {3, 300});
    layer.insert(4, {4, 400}); // freeze again

    EXPECT_EQ(layer.immutable_count(), 2);
}

TEST(MemoryLayerTest, LargeVolume) {
    const int N = 1000;
    MemoryLayer layer(100);

    for (int i = 0; i < N; ++i) {
        layer.insert(i, {i, i * 10});
    }

    EXPECT_EQ(layer.immutable_count(), N / 100);
}
