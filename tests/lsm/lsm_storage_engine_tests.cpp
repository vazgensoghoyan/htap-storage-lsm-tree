#include "storage/lsm/lsm_storage_engine.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "lsmtree/sstable/format/sst_layout.hpp"

namespace htap::storage {
namespace {

Schema MakeSchema() {
    return Schema({
        Column{
            .name = "id",
            .type = ValueType::INT64,
            .is_key = true,
            .nullable = false,
        },
        Column{
            .name = "age",
            .type = ValueType::INT64,
            .is_key = false,
            .nullable = false,
        },
        Column{
            .name = "name",
            .type = ValueType::STRING,
            .is_key = false,
            .nullable = false,
        },
    });
}

NullableValue IntValue(std::int64_t value) {
    return NullableValue{Value{value}};
}

NullableValue StringValue(std::string value) {
    return NullableValue{Value{std::move(value)}};
}

Row MakeRow(Key key, std::int64_t age, std::string name) {
    return Row{
        IntValue(key),
        IntValue(age),
        StringValue(std::move(name)),
    };
}

std::filesystem::path MakeTempDir(const std::string& test_name) {
    auto path = std::filesystem::temp_directory_path()
        / ("htap_lsm_storage_engine_" + test_name);

    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);

    return path;
}

std::vector<Key> CollectKeys(ICursor& cursor) {
    std::vector<Key> keys;

    while (cursor.valid()) {
        keys.push_back(cursor.key());
        cursor.next();
    }

    return keys;
}

std::vector<std::filesystem::path> SstableDirs(const std::filesystem::path& table_dir) {
    std::vector<std::filesystem::path> dirs;

    for (const auto& entry : std::filesystem::directory_iterator(table_dir)) {
        if (entry.is_directory() && entry.path().extension() == ".sst") {
            dirs.push_back(entry.path());
        }
    }

    return dirs;
}

std::uint32_t SparseIndexEntryCount(const std::filesystem::path& sst_dir) {
    std::ifstream input(sst_dir / "sparse.idx", std::ios::binary | std::ios::ate);
    if (!input.is_open()) {
        return 0;
    }

    constexpr std::streamoff entry_size = sizeof(Key) + sizeof(std::uint32_t);
    return static_cast<std::uint32_t>(input.tellg() / entry_size);
}

} 

TEST(LSMStorageEngineTest, CreateTableStoresSchema) {
    const auto dir = MakeTempDir("create_table_stores_schema");

    LSMStorageEngine storage(StorageConfig{.root_path = dir.string()});

    storage.create_table("users", MakeSchema());

    EXPECT_TRUE(storage.table_exists("users"));
    EXPECT_FALSE(storage.table_exists("missing"));

    const auto& schema = storage.get_table_schema("users");

    ASSERT_EQ(schema.size(), 3);
    EXPECT_EQ(schema.get_column(0).name, "id");
    EXPECT_EQ(schema.get_column(1).name, "age");
    EXPECT_EQ(schema.get_column(2).name, "name");

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, ScanReturnsInsertedRows) {
    const auto dir = MakeTempDir("scan_returns_inserted_rows");

    LSMStorageEngine storage(StorageConfig{.root_path = dir.string()});
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(3, 30, "c"));
    storage.insert("users", MakeRow(1, 10, "a"));
    storage.insert("users", MakeRow(2, 20, "b"));

    auto cursor = storage.scan(
        "users",
        std::nullopt,
        std::nullopt,
        {0, 1, 2},
        ScanOrder::KeyAscending
    );

    EXPECT_EQ(CollectKeys(*cursor), std::vector<Key>({1, 2, 3}));

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, ScanRespectsRange) {
    const auto dir = MakeTempDir("scan_respects_range");

    LSMStorageEngine storage(StorageConfig{.root_path = dir.string()});
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(1, 10, "a"));
    storage.insert("users", MakeRow(2, 20, "b"));
    storage.insert("users", MakeRow(3, 30, "c"));
    storage.insert("users", MakeRow(4, 40, "d"));

    auto cursor = storage.scan(
        "users",
        2,
        4,
        {0, 1},
        ScanOrder::KeyAscending
    );

    EXPECT_EQ(CollectKeys(*cursor), std::vector<Key>({2, 3}));

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, GetReturnsSingleExistingRow) {
    const auto dir = MakeTempDir("get_returns_single_existing_row");

    LSMStorageEngine storage(StorageConfig{.root_path = dir.string()});
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(1, 10, "a"));
    storage.insert("users", MakeRow(2, 20, "b"));
    storage.insert("users", MakeRow(3, 30, "c"));

    auto cursor = storage.get("users", 2, {0, 1});

    ASSERT_TRUE(cursor->valid());
    EXPECT_EQ(cursor->key(), 2);

    const auto age = cursor->value(1);

    ASSERT_TRUE(age.has_value());
    ASSERT_TRUE(std::holds_alternative<std::int64_t>(*age));
    EXPECT_EQ(std::get<std::int64_t>(*age), 20);

    cursor->next();
    EXPECT_FALSE(cursor->valid());

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, GetMissingKeyReturnsEmptyCursor) {
    const auto dir = MakeTempDir("get_missing_key_returns_empty_cursor");

    LSMStorageEngine storage(StorageConfig{.root_path = dir.string()});
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(1, 10, "a"));
    storage.insert("users", MakeRow(3, 30, "c"));

    auto cursor = storage.get("users", 2, {0, 1});

    EXPECT_FALSE(cursor->valid());

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, ProjectionAllowsReadingSelectedColumns) {
    const auto dir = MakeTempDir("projection_allows_reading_selected_columns");

    LSMStorageEngine storage(StorageConfig{.root_path = dir.string()});
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(1, 10, "a"));

    auto cursor = storage.scan(
        "users",
        std::nullopt,
        std::nullopt,
        {0, 1},
        ScanOrder::KeyAscending
    );

    ASSERT_TRUE(cursor->valid());

    EXPECT_EQ(cursor->key(), 1);

    const auto age = cursor->value(1);

    ASSERT_TRUE(age.has_value());
    ASSERT_TRUE(std::holds_alternative<std::int64_t>(*age));
    EXPECT_EQ(std::get<std::int64_t>(*age), 10);

    cursor->next();
    EXPECT_FALSE(cursor->valid());

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, CompactionUsesConfiguredColumnBlocksAndSparseIndex) {
    const auto dir = MakeTempDir("compaction_uses_configured_column_blocks");

    StorageConfig config;
    config.root_path = dir.string();
    config.memtable_threshold = 2;
    config.level0_compaction_trigger = 2;
    config.row_to_column_level = 1;
    config.sparse_index_step = 1;
    config.row_block_target_bytes = 1024;
    config.column_block_target_rows = 2;
    config.column_block_target_bytes = 1024;
    config.is_compaction_background = false;

    LSMStorageEngine storage(config);
    storage.create_table("users", MakeSchema());

    storage.insert("users", MakeRow(1, 10, "a"));
    storage.insert("users", MakeRow(2, 20, "b"));
    storage.insert("users", MakeRow(3, 30, "c"));
    storage.insert("users", MakeRow(4, 40, "d"));
    storage.wait_for_compaction("users");

    const auto sst_dirs = SstableDirs(dir / "users");
    ASSERT_EQ(sst_dirs.size(), 1u);

    std::ifstream info(sst_dirs.front() / "info.bin", std::ios::binary);
    ASSERT_TRUE(info.is_open());

    std::uint32_t num_blocks = 0;
    Key min_key = 0;
    Key max_key = 0;
    std::uint8_t layout = 0;

    info.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
    info.read(reinterpret_cast<char*>(&min_key), sizeof(min_key));
    info.read(reinterpret_cast<char*>(&max_key), sizeof(max_key));
    info.read(reinterpret_cast<char*>(&layout), sizeof(layout));

    EXPECT_EQ(num_blocks, 2u);
    EXPECT_EQ(min_key, 1);
    EXPECT_EQ(max_key, 4);
    EXPECT_EQ(layout, static_cast<std::uint8_t>(htap::lsmtree::sstable::SSTLayout::COLUMN));
    EXPECT_EQ(SparseIndexEntryCount(sst_dirs.front()), 2u);
    EXPECT_TRUE(std::filesystem::exists(sst_dirs.front() / "stats.bin"));

    std::filesystem::remove_all(dir);
}

TEST(LSMStorageEngineTest, ThrowsForMissingTable) {
    const auto dir = MakeTempDir("throws_for_missing_table");

    LSMStorageEngine storage(StorageConfig{.root_path = dir.string()});

    EXPECT_THROW(
        storage.scan("missing", std::nullopt, std::nullopt, {0}, ScanOrder::Unordered),
        std::runtime_error
    );

    EXPECT_THROW(
        storage.get("missing", 1, {0}),
        std::runtime_error
    );

    std::filesystem::remove_all(dir);
}

} 
