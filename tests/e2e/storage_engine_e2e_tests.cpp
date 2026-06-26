#include "storage/lsm/lsm_storage_engine.hpp"
#include "storage/model/schema_builder.hpp"
#include "lsmtree/sstable/format/sst_layout.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

namespace htap::storage {
namespace {

constexpr const char* kTableName = "events";

Schema MakeSchema() {
    return SchemaBuilder{}
        .add_column("id", ValueType::INT64, true, false)
        .add_column("value", ValueType::INT64, false, false)
        .add_column("score", ValueType::DOUBLE, false, false)
        .add_column("name", ValueType::STRING, false, false)
        .build();
}

Row MakeRow(Key key) {
    Row row(4);
    row[0] = key;
    row[1] = static_cast<std::int64_t>(key * 10);
    row[2] = static_cast<double>(key) * 0.5;
    row[3] = std::string("name_") + std::to_string(key);
    return row;
}

StorageConfig MakeE2EConfig(const fs::path& root) {
    StorageConfig config;
    config.root_path = root.string();
    config.memtable_threshold = 4;
    config.level0_compaction_trigger = 2;
    config.size_ratio = 2;
    config.base_level_size_bytes = 1;
    config.row_to_column_level = 2;
    config.sparse_index_step = 1;
    return config;
}

fs::path MakeTempDir(const std::string& test_name) {
    auto path = fs::temp_directory_path() / ("htap_e2e_" + test_name);
    fs::remove_all(path);
    fs::create_directories(path);
    return path;
}

void InsertRows(LSMStorageEngine& storage, Key from, Key to_exclusive) {
    for (Key key = from; key < to_exclusive; ++key) {
        storage.insert(kTableName, MakeRow(key));
    }
}

std::vector<Key> CollectKeys(ICursor& cursor) {
    std::vector<Key> keys;
    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }
    return keys;
}

std::vector<Key> ExpectedKeys(Key from, Key to_exclusive) {
    std::vector<Key> keys;
    for (Key key = from; key < to_exclusive; ++key) {
        keys.push_back(key);
    }
    return keys;
}

std::uint32_t ReadU32(std::ifstream& in) {
    std::uint32_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

std::int64_t ReadI64(std::ifstream& in) {
    std::int64_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

std::uint8_t ReadU8(std::ifstream& in) {
    std::uint8_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

bool HasColumnSSTable(const fs::path& table_dir) {
    if (!fs::exists(table_dir)) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(table_dir)) {
        if (!entry.is_directory()) {
            continue;
        }

        const fs::path info_path = entry.path() / "info.bin";
        if (!fs::exists(info_path)) {
            continue;
        }

        std::ifstream in(info_path, std::ios::binary);
        if (!in.is_open()) {
            continue;
        }

        [[maybe_unused]] const auto num_blocks = ReadU32(in);
        [[maybe_unused]] const auto min_key = ReadI64(in);
        [[maybe_unused]] const auto max_key = ReadI64(in);
        const auto layout = ReadU8(in);

        if (layout == static_cast<std::uint8_t>(lsmtree::sstable::SSTLayout::COLUMN)) {
            return true;
        }
    }

    return false;
}

class StorageEngineE2ETest : public ::testing::Test {
protected:
    void TearDown() override {
        if (!root_.empty()) {
            fs::remove_all(root_);
        }
    }

    fs::path ResetRoot(const std::string& test_name) {
        root_ = MakeTempDir(test_name);
        return root_;
    }

private:
    fs::path root_;
};

} // namespace

TEST_F(StorageEngineE2ETest, FlushThenReadReturnsAllRowsFromSSTables) {
    const auto root = ResetRoot("flush_then_read");

    LSMStorageEngine storage(MakeE2EConfig(root));
    storage.create_table(kTableName, MakeSchema());

    InsertRows(storage, 1, 17);

    auto cursor = storage.scan(
        kTableName,
        std::nullopt,
        std::nullopt,
        {0, 1, 2, 3},
        ScanOrder::KeyAscending
    );

    ASSERT_NE(cursor, nullptr);
    EXPECT_EQ(CollectKeys(*cursor), ExpectedKeys(1, 17));
}

TEST_F(StorageEngineE2ETest, CompactionToColumnSSTableRoundtripsProjectionAndPointLookup) {
    const auto root = ResetRoot("compaction_to_column");

    LSMStorageEngine storage(MakeE2EConfig(root));
    storage.create_table(kTableName, MakeSchema());

    InsertRows(storage, 1, 65);

    ASSERT_TRUE(HasColumnSSTable(root / kTableName))
        << "Expected cascade compaction to create at least one COLUMN SSTable";

    auto scan_cursor = storage.scan(
        kTableName,
        10,
        21,
        {0, 2},
        ScanOrder::KeyAscending
    );

    ASSERT_NE(scan_cursor, nullptr);

    Key expected_key = 10;
    while (scan_cursor->valid()) {
        EXPECT_EQ(scan_cursor->key(), expected_key);

        const auto score = scan_cursor->value(2);
        ASSERT_TRUE(score.has_value());
        ASSERT_TRUE(std::holds_alternative<double>(*score));
        EXPECT_DOUBLE_EQ(std::get<double>(*score), static_cast<double>(expected_key) * 0.5);

        ++expected_key;
        scan_cursor->next();
    }
    EXPECT_EQ(expected_key, 21);

    auto point_cursor = storage.get(kTableName, 42, {0, 1, 3});

    ASSERT_NE(point_cursor, nullptr);
    ASSERT_TRUE(point_cursor->valid());
    EXPECT_EQ(point_cursor->key(), 42);

    const auto value = point_cursor->value(1);
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::int64_t>(*value));
    EXPECT_EQ(std::get<std::int64_t>(*value), 420);

    const auto name = point_cursor->value(3);
    ASSERT_TRUE(name.has_value());
    ASSERT_TRUE(std::holds_alternative<std::string>(*name));
    EXPECT_EQ(std::get<std::string>(*name), "name_42");

    point_cursor->next();
    EXPECT_FALSE(point_cursor->valid());
}

TEST_F(StorageEngineE2ETest, MixedLevelsAndMemtableScanReturnsSortedUniqueRows) {
    const auto root = ResetRoot("mixed_levels_and_memtable");

    LSMStorageEngine storage(MakeE2EConfig(root));
    storage.create_table(kTableName, MakeSchema());

    InsertRows(storage, 1, 66);

    auto cursor = storage.scan(
        kTableName,
        5,
        63,
        {0, 1},
        ScanOrder::KeyAscending
    );

    ASSERT_NE(cursor, nullptr);
    EXPECT_EQ(CollectKeys(*cursor), ExpectedKeys(5, 63));
}

TEST_F(StorageEngineE2ETest, RestartWithSameSchemaLoadsManifestAndReadsSSTables) {
    const auto root = ResetRoot("restart_with_manifest");
    const auto schema = MakeSchema();

    {
        LSMStorageEngine storage(MakeE2EConfig(root));
        storage.create_table(kTableName, schema);
        InsertRows(storage, 1, 33);
    }

    {
        LSMStorageEngine storage(MakeE2EConfig(root));
        storage.create_table(kTableName, schema);

        auto cursor = storage.scan(
            kTableName,
            std::nullopt,
            std::nullopt,
            {0, 1},
            ScanOrder::KeyAscending
        );

        ASSERT_NE(cursor, nullptr);
        EXPECT_EQ(CollectKeys(*cursor), ExpectedKeys(1, 33));
    }
}

} // namespace htap::storage
