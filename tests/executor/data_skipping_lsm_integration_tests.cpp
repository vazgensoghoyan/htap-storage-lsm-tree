#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "executor/binder.hpp"
#include "executor/executor.hpp"
#include "parser/ast.hpp"
#include "storage/lsm/lsm_storage_engine.hpp"
#include "storage/model/schema_builder.hpp"

using namespace htap;

namespace {

class TempDir {
public:
    explicit TempDir(std::string name)
        : path_(std::filesystem::temp_directory_path() / std::move(name)) {
        cleanup();
        std::filesystem::create_directories(path_);
    }

    ~TempDir() {
        cleanup();
    }

    const std::filesystem::path& path() const {
        return path_;
    }

private:
    void cleanup() const {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

private:
    std::filesystem::path path_;
};

storage::Schema make_schema() {
    return storage::SchemaBuilder()
        .add_column("id", storage::ValueType::INT64, true, false)
        .add_column("age", storage::ValueType::INT64, false, false)
        .add_column("score", storage::ValueType::DOUBLE, false, true)
        .add_column("name", storage::ValueType::STRING, false, true)
        .build();
}

std::unique_ptr<parser::Expression> column(std::string name) {
    auto expr = std::make_unique<parser::ColumnExpression>();
    expr->column_name = std::move(name);
    return expr;
}

std::unique_ptr<parser::Expression> int_literal(std::int64_t value) {
    auto expr = std::make_unique<parser::IntLiteral>();
    expr->value = value;
    return expr;
}

std::unique_ptr<parser::Expression> binary(
    parser::BinaryOperation operation,
    std::unique_ptr<parser::Expression> left,
    std::unique_ptr<parser::Expression> right
) {
    auto expr = std::make_unique<parser::BinaryExpression>();
    expr->operation = operation;
    expr->left = std::move(left);
    expr->right = std::move(right);
    return expr;
}

std::unique_ptr<parser::SelectItem> select_item(std::unique_ptr<parser::Expression> expression) {
    auto item = std::make_unique<parser::SelectItemExpression>();
    item->expression = std::move(expression);
    return item;
}

}

TEST(DataSkippingLsmIntegrationTest, ExecutorQueryReturnsCorrectRowsWithSSTableStatsPresent) {
    TempDir dir("htap_data_skipping_lsm_integration");
    storage::LSMStorageEngine storage(storage::StorageConfig{
        .root_path = dir.path().string(),
        .memtable_threshold = 50,
    });
    storage.create_table("users", make_schema());

    for (std::int64_t id = 0; id < 120; ++id) {
        storage::Row row;
        row.push_back(id);
        row.push_back(id);
        row.push_back(static_cast<double>(id) / 10.0);
        row.push_back(std::string("user_" + std::to_string(id)));
        storage.insert("users", row);
    }

    storage.wait_for_compaction("users");

    ASSERT_TRUE(std::filesystem::exists(dir.path() / "users" / "sst_00000000000000000000.sst" / "stats.bin"));

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(select_item(column("id")));
    statement.where_expression = binary(
        parser::BinaryOperation::GreaterEqual,
        column("age"),
        int_literal(75)
    );

    executor::Binder binder(storage);
    executor::Executor executor(storage);
    const auto bound = binder.Bind(statement);
    const auto execution_result = executor.Execute(*bound);
    const auto* result = std::get_if<executor::SelectResult>(&execution_result);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->rows.size(), 45u);

    std::vector<std::int64_t> ids;
    ids.reserve(result->rows.size());
    for (const auto& row : result->rows) {
        ASSERT_TRUE(row[0].has_value());
        ids.push_back(std::get<std::int64_t>(*row[0]));
    }

    std::sort(ids.begin(), ids.end());

    for (std::int64_t expected = 75; expected < 120; ++expected) {
        EXPECT_EQ(ids[static_cast<std::size_t>(expected - 75)], expected);
    }
}
