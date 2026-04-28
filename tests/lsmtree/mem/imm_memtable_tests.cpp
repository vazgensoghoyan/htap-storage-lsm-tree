#include <gtest/gtest.h>

#include "lsmtree/mem/imm_memtable.hpp"
#include "lsmtree/mem/cursors/merge_cursor.hpp"

#include <map>
#include <random>

using namespace htap::lsmtree;
using namespace htap::storage;

TEST(ImmutableMemTableTest, GetExistingKey) {
    std::vector<Row> data;

    data.push_back(Row{{Key(1), 10}});
    data.push_back(Row{{Key(2), 20}});
    data.push_back(Row{{Key(3), 30}});

    ImmutableMemTable table(std::move(data));

    auto cursor = table.get(Key(2), {});

    ASSERT_NE(cursor, nullptr);
    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), Key(2));
}

TEST(ImmutableMemTableTest, GetMissingKey) {
    std::vector<Row> data = {
        Row{{Key(1), 10}},
        Row{{Key(2), 20}}
    };

    ImmutableMemTable table(std::move(data));

    auto cursor = table.get(Key(999), {});

    EXPECT_EQ(cursor, nullptr);
}

TEST(ImmutableMemTableTest, GetEdgeKeys) {
    std::vector<Row> data = {
        Row{{Key(1), 10}},
        Row{{Key(2), 20}},
        Row{{Key(3), 30}}
    };

    ImmutableMemTable table(std::move(data));

    auto c1 = table.get(Key(1), {});
    auto c2 = table.get(Key(3), {});

    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    EXPECT_EQ(c1->key(), Key(1));
    EXPECT_EQ(c2->key(), Key(3));
}

TEST(ImmutableMemTableTest, ScanFullRange) {
    std::vector<Row> data = {
        Row{{Key(1), 10}},
        Row{{Key(2), 20}},
        Row{{Key(3), 30}},
        Row{{Key(4), 40}}
    };

    ImmutableMemTable table(std::move(data));

    auto cursor = table.scan(std::nullopt, std::nullopt, {});

    std::vector<Key> result;

    while (cursor && cursor->valid()) {
        result.push_back(cursor->key());
        cursor->next();
    }

    EXPECT_EQ(result, (std::vector<Key>{1,2,3,4}));
}

TEST(ImmutableMemTableTest, ScanFrom) {
    std::vector<Row> data = {
        Row{{Key(1), 10}},
        Row{{Key(2), 20}},
        Row{{Key(3), 30}},
        Row{{Key(4), 40}}
    };

    ImmutableMemTable table(std::move(data));

    auto cursor = table.scan(Key(2), std::nullopt, {});

    std::vector<Key> result;

    while (cursor->valid()) {
        result.push_back(cursor->key());
        cursor->next();
    }

    EXPECT_EQ(result, (std::vector<Key>{2,3,4}));
}

TEST(ImmutableMemTableTest, ScanToExclusive) {
    std::vector<Row> data = {
        Row{{Key(1), 10}},
        Row{{Key(2), 20}},
        Row{{Key(3), 30}},
        Row{{Key(4), 40}}
    };

    ImmutableMemTable table(std::move(data));

    auto cursor = table.scan(std::nullopt, Key(3), {});

    std::vector<Key> result;

    while (cursor->valid()) {
        result.push_back(cursor->key());
        cursor->next();
    }

    EXPECT_EQ(result, (std::vector<Key>{1,2}));
}

TEST(ImmutableMemTableTest, ScanRange) {
    std::vector<Row> data = {
        Row{{Key(1), 10}},
        Row{{Key(2), 20}},
        Row{{Key(3), 30}},
        Row{{Key(4), 40}},
        Row{{Key(5), 50}}
    };

    ImmutableMemTable table(std::move(data));

    auto cursor = table.scan(Key(2), Key(5), {});

    std::vector<Key> result;

    while (cursor->valid()) {
        result.push_back(cursor->key());
        cursor->next();
    }

    EXPECT_EQ(result, (std::vector<Key>{2,3,4}));
}

TEST(ImmutableMemTableTest, EmptyTable) {
    std::vector<Row> data;

    ImmutableMemTable table(std::move(data));

    EXPECT_EQ(table.size(), 0);

    auto c1 = table.get(Key(1), {});
    auto c2 = table.scan(std::nullopt, std::nullopt, {});

    EXPECT_EQ(c1, nullptr);
    EXPECT_TRUE(c2 == nullptr || !c2->valid());
}

TEST(ImmutableMemTableTest, Size) {
    std::vector<Row> data = {
        Row{{Key(1), 10}},
        Row{{Key(2), 20}}
    };

    ImmutableMemTable table(std::move(data));

    EXPECT_EQ(table.size(), 2);
}

TEST(ImmutableMemTableTest, ProjectionDoesNotAffectIteration) {
    std::vector<Row> data = {
        Row{{Key(1), 10, 100}},
        Row{{Key(2), 20, 200}}
    };

    ImmutableMemTable table(std::move(data));

    auto cursor = table.scan(std::nullopt, std::nullopt, {0,2});

    std::vector<Key> result;

    while (cursor->valid()) {
        result.push_back(cursor->key());

        // проверка value через projection
        auto v = cursor->value(2);
        EXPECT_TRUE(v.has_value());

        cursor->next();
    }

    EXPECT_EQ(result, (std::vector<Key>{1,2}));
}

// RANDOMIZED

static Key rand_key(std::mt19937& gen) {
    return Key(gen() % 1000);
}

TEST(ImmutableMemTableProperty, RandomGetMatchesMap) {
    std::mt19937 gen(42);

    std::multimap<Key, int> oracle;
    std::vector<Row> data;

    // генерим "истину"
    for (int i = 0; i < 200; i++) {
        Key k = rand_key(gen);
        oracle.insert({k, i});

        data.push_back(Row{{k, i}});
    }

    // важно: сортируем как должно быть в memtable
    std::sort(data.begin(), data.end(),
        [](const Row& a, const Row& b) {
            return std::get<Key>(a[KEY_COLUMN_INDEX].value()) < std::get<Key>(b[KEY_COLUMN_INDEX].value());
        });

    ImmutableMemTable table(std::move(data));

    for (const auto& [k, v] : oracle) {
        auto cursor = table.get(k, {});

        ASSERT_NE(cursor, nullptr);
        ASSERT_TRUE(cursor->valid());
        EXPECT_EQ(cursor->key(), k);
    }
}

TEST(ImmutableMemTableProperty, RandomScanMatchesMap) {
    std::mt19937 gen(123);

    std::multimap<Key, int> oracle;
    std::vector<Row> data;

    for (int i = 0; i < 300; i++) {
        Key k = Key(gen() % 1000);
        oracle.insert({k, i});
        data.push_back(Row{{k, i}});
    }

    std::sort(data.begin(), data.end(),
        [](const Row& a, const Row& b) {
            return std::get<Key>(a[KEY_COLUMN_INDEX].value()) < std::get<Key>(b[KEY_COLUMN_INDEX].value());
        });

    ImmutableMemTable table(std::move(data));

    for (int t = 0; t < 100; t++) {
        Key from = Key(gen() % 1000);
        Key to   = Key(gen() % 1000);

        if (from > to) std::swap(from, to);

        auto cursor = table.scan(from, to, {});

        std::vector<Key> result;

        while (cursor && cursor->valid()) {
            result.push_back(cursor->key());
            cursor->next();
        }

        std::vector<Key> expected;
        for (auto it = oracle.lower_bound(from);
            it != oracle.end() && it->first < to;
            ++it) {
            expected.push_back(it->first);
        }

        EXPECT_EQ(result, expected);
    }
}

// MERGE CURSOR CONSISTENCY

TEST(ImmutableMemTableProperty, MergeCursorConsistency) {
    std::vector<Row> data1 = {
        Row{{Key(1), 10}},
        Row{{Key(3), 30}},
        Row{{Key(5), 50}}
    };

    std::vector<Row> data2 = {
        Row{{Key(2), 20}},
        Row{{Key(4), 40}},
        Row{{Key(6), 60}}
    };

    ImmutableMemTable t1(std::move(data1));
    ImmutableMemTable t2(std::move(data2));

    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(t1.scan(std::nullopt, std::nullopt, {}));
    cursors.push_back(t2.scan(std::nullopt, std::nullopt, {}));

    MergeCursor mc(std::move(cursors));

    std::vector<Key> result;

    while (mc.valid()) {
        result.push_back(mc.key());
        mc.next();
    }

    EXPECT_EQ(result, (std::vector<Key>{1,2,3,4,5,6}));
}

TEST(ImmutableMemTableProperty, MergeHandlesDuplicates) {
    std::vector<Row> data1 = {
        Row{{Key(1), 10}},
        Row{{Key(2), 20}},
        Row{{Key(3), 30}}
    };

    std::vector<Row> data2 = {
        Row{{Key(2), 200}},
        Row{{Key(3), 300}},
        Row{{Key(4), 400}}
    };

    ImmutableMemTable t1(std::move(data1));
    ImmutableMemTable t2(std::move(data2));

    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(t1.scan(std::nullopt, std::nullopt, {}));
    cursors.push_back(t2.scan(std::nullopt, std::nullopt, {}));

    MergeCursor mc(std::move(cursors));

    std::vector<Key> result;

    while (mc.valid()) {
        result.push_back(mc.key());
        mc.next();
    }

    // без дедупликации ожидаем:
    EXPECT_EQ(result, (std::vector<Key>{1,2,3,4}));
}

TEST(ImmutableMemTableProperty, MergeIsSorted) {
    std::vector<Row> a = {
        Row{{Key(10), 1}},
        Row{{Key(30), 2}}
    };

    std::vector<Row> b = {
        Row{{Key(20), 3}},
        Row{{Key(40), 4}}
    };

    ImmutableMemTable t1(std::move(a));
    ImmutableMemTable t2(std::move(b));

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(t1.scan(std::nullopt, std::nullopt, {}));
    cursors.push_back(t2.scan(std::nullopt, std::nullopt, {}));

    MergeCursor mc(std::move(cursors));

    Key prev = 0;
    bool first = true;

    while (mc.valid()) {
        Key k = mc.key();

        if (!first) {
            EXPECT_LE(prev, k);
        }

        prev = k;
        first = false;

        mc.next();
    }
}
