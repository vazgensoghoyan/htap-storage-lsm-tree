#include "storage/cursor/cursor_factory.hpp"

#include "storage/cursor/empty_cursor.hpp"
#include "storage/mock/mock_cursor.hpp"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace htap::storage {
namespace {

std::map<Key, Row> make_rows(std::vector<Key> keys) {
    std::map<Key, Row> rows;

    for (Key key : keys) {
        rows.emplace(key, Row{});
    }

    return rows;
}

std::unique_ptr<ICursor> make_mock_cursor(
    const std::map<Key, Row>& rows,
    OptKey from = std::nullopt,
    OptKey to = std::nullopt
) {
    return std::make_unique<MockCursor>(&rows, from, to);
}

std::vector<Key> read_all_keys(ICursor& cursor) {
    std::vector<Key> result;

    while (cursor.valid()) {
        result.push_back(cursor.key());
        cursor.next();
    }

    return result;
}

} 

TEST(CursorFactoryTest, ReturnsEmptyCursorForNoSources) {
    std::vector<std::unique_ptr<ICursor>> cursors;

    auto result = cursor::compose_cursors(
        std::move(cursors),
        ScanOrder::Unordered
    );

    EXPECT_FALSE(result->valid());
}

TEST(CursorFactoryTest, ReturnsEmptyCursorForOnlyInvalidSources) {
    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(std::make_unique<cursor::EmptyCursor>());
    cursors.push_back(std::make_unique<cursor::EmptyCursor>());

    auto result = cursor::compose_cursors(
        std::move(cursors),
        ScanOrder::Unordered
    );

    EXPECT_FALSE(result->valid());
}

TEST(CursorFactoryTest, ReturnsSingleValidCursorBehavior) {
    auto rows = make_rows({1, 2, 3});

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(make_mock_cursor(rows));

    auto result = cursor::compose_cursors(
        std::move(cursors),
        ScanOrder::Unordered
    );

    EXPECT_EQ(read_all_keys(*result), std::vector<Key>({1, 2, 3}));
}

TEST(CursorFactoryTest, IgnoresInvalidSourcesAndReturnsSingleValidStream) {
    auto rows = make_rows({10, 20});

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(std::make_unique<cursor::EmptyCursor>());
    cursors.push_back(make_mock_cursor(rows));
    cursors.push_back(std::make_unique<cursor::EmptyCursor>());

    auto result = cursor::compose_cursors(
        std::move(cursors),
        ScanOrder::KeyAscending
    );

    EXPECT_EQ(read_all_keys(*result), std::vector<Key>({10, 20}));
}

TEST(CursorFactoryTest, ChainsSourcesForUnorderedScan) {
    auto rows1 = make_rows({1, 2});
    auto rows2 = make_rows({10, 11});
    auto rows3 = make_rows({100});

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(make_mock_cursor(rows1));
    cursors.push_back(make_mock_cursor(rows2));
    cursors.push_back(make_mock_cursor(rows3));

    auto result = cursor::compose_cursors(
        std::move(cursors),
        ScanOrder::Unordered
    );

    EXPECT_EQ(
        read_all_keys(*result),
        std::vector<Key>({1, 2, 10, 11, 100})
    );
}

TEST(CursorFactoryTest, MergesSourcesForKeyAscendingScan) {
    auto rows1 = make_rows({1, 4, 7});
    auto rows2 = make_rows({2, 5, 8});
    auto rows3 = make_rows({3, 6, 9});

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(make_mock_cursor(rows1));
    cursors.push_back(make_mock_cursor(rows2));
    cursors.push_back(make_mock_cursor(rows3));

    auto result = cursor::compose_cursors(
        std::move(cursors),
        ScanOrder::KeyAscending
    );

    EXPECT_EQ(
        read_all_keys(*result),
        std::vector<Key>({1, 2, 3, 4, 5, 6, 7, 8, 9})
    );
}

TEST(CursorFactoryTest, IgnoresNullptrSources) {
    auto rows = make_rows({42});

    std::vector<std::unique_ptr<ICursor>> cursors;
    cursors.push_back(nullptr);
    cursors.push_back(make_mock_cursor(rows));
    cursors.push_back(nullptr);

    auto result = cursor::compose_cursors(
        std::move(cursors),
        ScanOrder::Unordered
    );

    EXPECT_EQ(read_all_keys(*result), std::vector<Key>({42}));
}

} 
