#include <gtest/gtest.h>

#include "lsmtree/mem/cursors/merge_cursor.hpp"
#include "storage/api/cursor_interface.hpp"

#include <vector>

class MockCursor : public htap::storage::ICursor {
public:
    explicit MockCursor(std::vector<htap::storage::Key> keys) : keys_(std::move(keys)) {}

    bool valid() const override {
        return idx_ < keys_.size();
    }

    void next() override {
        if (valid()) ++idx_;
    }

    htap::storage::Key key() const override {
        if (!valid()) throw std::runtime_error("invalid cursor");
        return keys_[idx_];
    }

    htap::storage::NullableValue value(size_t) const override {
        return {};
    }

private:
    std::vector<htap::storage::Key> keys_;
    size_t idx_ = 0;
};

using namespace htap::lsmtree;
using htap::storage::Key;

TEST(MergeCursorTest, BasicSortedMerge) {
    auto c1 = std::make_unique<MockCursor>(
        std::vector<Key>{1, 3, 5}
    );

    auto c2 = std::make_unique<MockCursor>(
        std::vector<Key>{2, 4, 6}
    );

    std::vector<std::unique_ptr<htap::storage::ICursor>> cursors;
    cursors.push_back(std::move(c1));
    cursors.push_back(std::move(c2));

    MergeCursor mc(std::move(cursors));

    std::vector<Key> result;

    for (; mc.valid(); mc.next()) {
        result.push_back(mc.key());
    }

    EXPECT_EQ(result, (std::vector<Key>{1,2,3,4,5,6}));
}

TEST(MergeCursorTest, EmptyCursors) {
    std::vector<std::unique_ptr<htap::storage::ICursor>> cursors;

    MergeCursor mc(std::move(cursors));

    EXPECT_FALSE(mc.valid());
}

TEST(MergeCursorTest, OneEmptyOneValid) {
    auto c1 = std::make_unique<MockCursor>(
        std::vector<Key>{}
    );

    auto c2 = std::make_unique<MockCursor>(
        std::vector<Key>{10, 20}
    );

    std::vector<std::unique_ptr<htap::storage::ICursor>> cursors;
    cursors.push_back(std::move(c1));
    cursors.push_back(std::move(c2));

    MergeCursor mc(std::move(cursors));

    std::vector<Key> result;

    while (mc.valid()) {
        result.push_back(mc.key());
        mc.next();
    }

    EXPECT_EQ(result, (std::vector<Key>{10,20}));
}

TEST(MergeCursorTest, DuplicatesAcrossCursors) {
    auto c1 = std::make_unique<MockCursor>(
        std::vector<Key>{1, 2, 5}
    );

    auto c2 = std::make_unique<MockCursor>(
        std::vector<Key>{2, 3, 5}
    );

    std::vector<std::unique_ptr<htap::storage::ICursor>> cursors;
    cursors.push_back(std::move(c1));
    cursors.push_back(std::move(c2));

    MergeCursor mc(std::move(cursors));

    std::vector<Key> result;

    while (mc.valid()) {
        result.push_back(mc.key());
        mc.next();
    }

    EXPECT_EQ(result, (std::vector<Key>{1,2,3,5}));
}

TEST(MergeCursorTest, MonotonicOrder) {
    auto c1 = std::make_unique<MockCursor>(
        std::vector<Key>{1, 10, 20}
    );

    auto c2 = std::make_unique<MockCursor>(
        std::vector<Key>{2, 3, 30}
    );

    std::vector<std::unique_ptr<htap::storage::ICursor>> cursors;
    cursors.push_back(std::move(c1));
    cursors.push_back(std::move(c2));

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
