#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "executor/binder.hpp"
#include "executor/executor.hpp"
#include "parser/ast.hpp"
#include "storage/mock/mock_storage_engine.hpp"
#include "storage/model/schema_builder.hpp"

using namespace htap;

namespace {

storage::Schema MakeUsersSchema() {
    return storage::SchemaBuilder()
        .add_column("id", storage::ValueType::INT64, true, false)
        .add_column("name", storage::ValueType::STRING, false, true)
        .add_column("age", storage::ValueType::INT64, false, false)
        .add_column("salary", storage::ValueType::DOUBLE, false, true)
        .build();
}

storage::MockStorageEngine MakeEngineWithUsers() {
    storage::MockStorageEngine engine;
    engine.create_table("users", MakeUsersSchema());
    return engine;
}

void InsertUser(
    storage::MockStorageEngine& engine,
    std::int64_t id,
    std::optional<std::string> name,
    std::int64_t age,
    std::optional<double> salary
) {
    storage::Row row;
    row.push_back(id);

    if (name.has_value()) {
        row.push_back(*name);
    } else {
        row.push_back(std::nullopt);
    }

    row.push_back(age);

    if (salary.has_value()) {
        row.push_back(*salary);
    } else {
        row.push_back(std::nullopt);
    }

    engine.insert("users", row);
}

storage::MockStorageEngine MakeEngineWithUsersData() {
    auto engine = MakeEngineWithUsers();

    InsertUser(engine, 1, std::string("Ann"), 19, 1000.0);
    InsertUser(engine, 2, std::string("Bob"), 25, std::nullopt);
    InsertUser(engine, 3, std::nullopt, 17, 2000.0);
    InsertUser(engine, 4, std::string("Carl"), 19, 1500.0);
    InsertUser(engine, 40, std::string("Dasha"), 19, 5000.0);

    return engine;
}

std::unique_ptr<parser::ColumnExpression> MakeColumnExpr(const std::string& name) {
    auto expr = std::make_unique<parser::ColumnExpression>();
    expr->column_name = name;
    return expr;
}

std::unique_ptr<parser::IntLiteral> MakeIntLiteral(std::int64_t value) {
    auto expr = std::make_unique<parser::IntLiteral>();
    expr->value = value;
    return expr;
}

std::unique_ptr<parser::BinaryExpression> MakeBinaryExpr(
    parser::BinaryOperation op,
    std::unique_ptr<parser::Expression> left,
    std::unique_ptr<parser::Expression> right
) {
    auto expr = std::make_unique<parser::BinaryExpression>();
    expr->operation = op;
    expr->left = std::move(left);
    expr->right = std::move(right);
    return expr;
}

std::unique_ptr<parser::SelectItemExpression> MakeSelectItemExpr(
    std::unique_ptr<parser::Expression> expr
) {
    auto item = std::make_unique<parser::SelectItemExpression>();
    item->expression = std::move(expr);
    return item;
}

std::unique_ptr<parser::SelectAggregateExpression> MakeSelectItemAggregate(
    parser::AggregateKind kind,
    std::unique_ptr<parser::Expression> expr
) {
    auto item = std::make_unique<parser::SelectAggregateExpression>();
    item->kind = kind;
    item->expression = std::move(expr);
    return item;
}

executor::SelectResult ExecuteSelect(
    storage::MockStorageEngine& engine,
    parser::SelectStatement& statement
) {
    executor::Binder binder(engine);
    executor::Executor executor(engine);

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    executor::ExecutionResult result = executor.Execute(*bound);

    auto* select = std::get_if<executor::SelectResult>(&result);
    if (!select) {
        throw std::runtime_error("Expected SelectResult");
    }

    return *select;
}

std::int64_t GetInt(const executor::ResultRow& row, std::size_t idx) {
    EXPECT_TRUE(row[idx].has_value());
    return std::get<std::int64_t>(*row[idx]);
}

double GetDouble(const executor::ResultRow& row, std::size_t idx) {
    EXPECT_TRUE(row[idx].has_value());
    return std::get<double>(*row[idx]);
}

} 

TEST(ExecutorGroupedAggregateSelect, GroupsByIntColumnAndCountsRows) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";

    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("age")));
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );

    statement.group_by_items.push_back(MakeColumnExpr("age"));

    executor::SelectResult result = ExecuteSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 3u);

    EXPECT_EQ(GetInt(result.rows[0], 0), 17);
    EXPECT_EQ(GetInt(result.rows[0], 1), 1);

    EXPECT_EQ(GetInt(result.rows[1], 0), 19);
    EXPECT_EQ(GetInt(result.rows[1], 1), 3);

    EXPECT_EQ(GetInt(result.rows[2], 0), 25);
    EXPECT_EQ(GetInt(result.rows[2], 1), 1);
}

TEST(ExecutorGroupedAggregateSelect, ComputesMultipleAggregatesPerGroup) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";

    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("age")));
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("salary"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Sum, MakeColumnExpr("salary"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Avg, MakeColumnExpr("salary"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Min, MakeColumnExpr("salary"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Max, MakeColumnExpr("salary"))
    );

    statement.group_by_items.push_back(MakeColumnExpr("age"));

    executor::SelectResult result = ExecuteSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 3u);

    EXPECT_EQ(GetInt(result.rows[0], 0), 17);
    EXPECT_EQ(GetInt(result.rows[0], 1), 1);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 2), 2000.0);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 3), 2000.0);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 4), 2000.0);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 5), 2000.0);

    EXPECT_EQ(GetInt(result.rows[1], 0), 19);
    EXPECT_EQ(GetInt(result.rows[1], 1), 3);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[1], 2), 7500.0);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[1], 3), 2500.0);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[1], 4), 1000.0);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[1], 5), 5000.0);

    EXPECT_EQ(GetInt(result.rows[2], 0), 25);
    EXPECT_EQ(GetInt(result.rows[2], 1), 0);
    EXPECT_FALSE(result.rows[2][2].has_value());
    EXPECT_FALSE(result.rows[2][3].has_value());
    EXPECT_FALSE(result.rows[2][4].has_value());
    EXPECT_FALSE(result.rows[2][5].has_value());
}

TEST(ExecutorGroupedAggregateSelect, AppliesWhereBeforeGrouping) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";

    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("age")));
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );

    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::Less,
        MakeColumnExpr("id"),
        MakeIntLiteral(5)
    );

    statement.group_by_items.push_back(MakeColumnExpr("age"));

    executor::SelectResult result = ExecuteSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 3u);

    EXPECT_EQ(GetInt(result.rows[0], 0), 17);
    EXPECT_EQ(GetInt(result.rows[0], 1), 1);

    EXPECT_EQ(GetInt(result.rows[1], 0), 19);
    EXPECT_EQ(GetInt(result.rows[1], 1), 2);

    EXPECT_EQ(GetInt(result.rows[2], 0), 25);
    EXPECT_EQ(GetInt(result.rows[2], 1), 1);
}
