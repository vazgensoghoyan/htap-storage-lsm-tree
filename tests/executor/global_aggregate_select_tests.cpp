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

    InsertUser(engine, 1,  std::string("Ann"),   19, 1000.0);
    InsertUser(engine, 2,  std::string("Bob"),   25, std::nullopt);
    InsertUser(engine, 3,  std::nullopt,         17, 2000.0);
    InsertUser(engine, 4,  std::string("Carl"),  19, 1500.0);
    InsertUser(engine, 40, std::string("Dasha"), 19, 5000.0);

    return engine;
}

std::unique_ptr<parser::Expression> MakeColumnExpr(const std::string& name) {
    auto expr = std::make_unique<parser::ColumnExpression>();
    expr->column_name = name;
    return expr;
}

std::unique_ptr<parser::Expression> MakeIntLiteral(std::int64_t value) {
    auto expr = std::make_unique<parser::IntLiteral>();
    expr->value = value;
    return expr;
}

std::unique_ptr<parser::Expression> MakeBinaryExpr(
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

std::unique_ptr<parser::SelectItem> MakeSelectItemAggregate(
    parser::AggregateKind kind,
    std::unique_ptr<parser::Expression> expr
) {
    auto item = std::make_unique<parser::SelectAggregateExpression>();
    item->kind = kind;
    item->expression = std::move(expr);
    return item;
}

std::unique_ptr<parser::Expression> MakeIsNullExpr(
    std::unique_ptr<parser::Expression> inner,
    bool is_not
) {
    auto expr = std::make_unique<parser::IsNullExpression>();
    expr->expression = std::move(inner);
    expr->is_not = is_not;
    return expr;
}

executor::SelectResult ExecuteAggregateSelect(
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

} // namespace

TEST(ExecutorGlobalAggregateSelect, CountsAllRows) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.column_names.size(), 1u);
    EXPECT_EQ(result.column_names[0], "count(id)");

    ASSERT_EQ(result.rows.size(), 1u);
    ASSERT_EQ(result.rows[0].size(), 1u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 5);
}

TEST(ExecutorGlobalAggregateSelect, CountsOnlyNonNullValues) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("salary"))
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 4);
}

TEST(ExecutorGlobalAggregateSelect, SumsIntColumn) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Sum, MakeColumnExpr("age"))
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 99);
}

TEST(ExecutorGlobalAggregateSelect, AveragesIntColumnAsDouble) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Avg, MakeColumnExpr("age"))
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 0), 19.8);
}

TEST(ExecutorGlobalAggregateSelect, ComputesMinAndMax) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Min, MakeColumnExpr("age"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Max, MakeColumnExpr("age"))
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    ASSERT_EQ(result.rows[0].size(), 2u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 17);
    EXPECT_EQ(GetInt(result.rows[0], 1), 25);
}

TEST(ExecutorGlobalAggregateSelect, AppliesWhereBeforeAggregation) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Sum, MakeColumnExpr("age"))
    );
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::Less,
        MakeColumnExpr("age"),
        MakeIntLiteral(20)
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    ASSERT_EQ(result.rows[0].size(), 2u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 4);
    EXPECT_EQ(GetInt(result.rows[0], 1), 74);
}

TEST(ExecutorGlobalAggregateSelect, ReturnsZeroAndNullsForEmptyInput) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Sum, MakeColumnExpr("age"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Avg, MakeColumnExpr("age"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Min, MakeColumnExpr("age"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Max, MakeColumnExpr("age"))
    );

    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::And,
        MakeBinaryExpr(
            parser::BinaryOperation::Equal,
            MakeColumnExpr("id"),
            MakeIntLiteral(1)
        ),
        MakeBinaryExpr(
            parser::BinaryOperation::Equal,
            MakeColumnExpr("id"),
            MakeIntLiteral(2)
        )
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    ASSERT_EQ(result.rows[0].size(), 5u);

    EXPECT_EQ(GetInt(result.rows[0], 0), 0);
    EXPECT_FALSE(result.rows[0][1].has_value());
    EXPECT_FALSE(result.rows[0][2].has_value());
    EXPECT_FALSE(result.rows[0][3].has_value());
    EXPECT_FALSE(result.rows[0][4].has_value());
}

TEST(ExecutorGlobalAggregateSelect, ReturnsNoRowsForLimitZero) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.limit = 0;

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    EXPECT_TRUE(result.rows.empty());
}

TEST(ExecutorGlobalAggregateSelect, UsesPointLookupPathForAggregate) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Sum, MakeColumnExpr("age"))
    );
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::Equal,
        MakeColumnExpr("id"),
        MakeIntLiteral(40)
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 19);
}

TEST(ExecutorGlobalAggregateSelect, UsesRangeScanPathForAggregate) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::And,
        MakeBinaryExpr(
            parser::BinaryOperation::GreaterEqual,
            MakeColumnExpr("id"),
            MakeIntLiteral(2)
        ),
        MakeBinaryExpr(
            parser::BinaryOperation::LessEqual,
            MakeColumnExpr("id"),
            MakeIntLiteral(4)
        )
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 3);
}

TEST(ExecutorGlobalAggregateSelect, SumsDoubleColumnIgnoringNulls) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Sum, MakeColumnExpr("salary"))
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 0), 9500.0);
}

TEST(ExecutorGlobalAggregateSelect, AveragesDoubleColumnIgnoringNulls) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Avg, MakeColumnExpr("salary"))
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 0), 2375.0);
}

TEST(ExecutorGlobalAggregateSelect, ComputesMinAndMaxForDoubleColumn) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Min, MakeColumnExpr("salary"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Max, MakeColumnExpr("salary"))
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    ASSERT_EQ(result.rows[0].size(), 2u);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 0), 1000.0);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 1), 5000.0);
}

TEST(ExecutorGlobalAggregateSelect, ReturnsZeroAndNullsWhenAllAggregateInputsAreNull) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
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
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::Equal,
        MakeColumnExpr("id"),
        MakeIntLiteral(2)
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    ASSERT_EQ(result.rows[0].size(), 5u);

    EXPECT_EQ(GetInt(result.rows[0], 0), 0);
    EXPECT_FALSE(result.rows[0][1].has_value());
    EXPECT_FALSE(result.rows[0][2].has_value());
    EXPECT_FALSE(result.rows[0][3].has_value());
    EXPECT_FALSE(result.rows[0][4].has_value());
}

TEST(ExecutorGlobalAggregateSelect, SupportsMultipleAggregatesInOneQuery) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Sum, MakeColumnExpr("age"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Avg, MakeColumnExpr("salary"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Min, MakeColumnExpr("age"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Max, MakeColumnExpr("salary"))
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    ASSERT_EQ(result.rows[0].size(), 5u);

    EXPECT_EQ(GetInt(result.rows[0], 0), 5);
    EXPECT_EQ(GetInt(result.rows[0], 1), 99);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 2), 2375.0);
    EXPECT_EQ(GetInt(result.rows[0], 3), 17);
    EXPECT_DOUBLE_EQ(GetDouble(result.rows[0], 4), 5000.0);
}

TEST(ExecutorGlobalAggregateSelect, CountsExpressionResults) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(
            parser::AggregateKind::Count,
            MakeIsNullExpr(MakeColumnExpr("name"), false)
        )
    );

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 5);
}

TEST(ExecutorGlobalAggregateSelect, ReturnsSingleRowWhenLimitIsGreaterThanOne) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.limit = 10;

    executor::SelectResult result = ExecuteAggregateSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 5);
}
