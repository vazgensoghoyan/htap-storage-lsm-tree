#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
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

std::unique_ptr<parser::Expression> MakeUnaryExpr(
    parser::UnaryOperation op,
    std::unique_ptr<parser::Expression> inner
) {
    auto expr = std::make_unique<parser::UnaryExpression>();
    expr->operation = op;
    expr->expression = std::move(inner);
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

std::unique_ptr<parser::Expression> MakeIsNullExpr(
    std::unique_ptr<parser::Expression> inner,
    bool is_not
) {
    auto expr = std::make_unique<parser::IsNullExpression>();
    expr->expression = std::move(inner);
    expr->is_not = is_not;
    return expr;
}

std::unique_ptr<parser::SelectItem> MakeSelectItemExpr(
    std::unique_ptr<parser::Expression> expr
) {
    auto item = std::make_unique<parser::SelectItemExpression>();
    item->expression = std::move(expr);
    return item;
}

parser::OrderByItem MakeOrderByItem(
    std::unique_ptr<parser::Expression> expr,
    parser::OrderDirection direction = parser::OrderDirection::Asc
) {
    parser::OrderByItem item;
    item.expression = std::move(expr);
    item.direction = direction;
    return item;
}

executor::SelectResult ExecutePlainSelect(
    storage::MockStorageEngine& engine,
    parser::SelectStatement& statement
) {
    executor::Binder binder(engine);
    executor::Executor exec(engine);

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    executor::ExecutionResult result = exec.Execute(*bound);

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

std::string GetString(const executor::ResultRow& row, std::size_t idx) {
    EXPECT_TRUE(row[idx].has_value());
    return std::get<std::string>(*row[idx]);
}

bool GetBool(const executor::ResultRow& row, std::size_t idx) {
    EXPECT_TRUE(row[idx].has_value());
    return std::get<bool>(*row[idx]);
}

} // namespace

TEST(ExecutorPlainSelect, SelectsProjectedColumnWithFullScan) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.column_names.size(), 1u);
    EXPECT_EQ(result.column_names[0], "name");

    ASSERT_EQ(result.rows.size(), 5u);

    EXPECT_EQ(GetString(result.rows[0], 0), "Ann");
    EXPECT_EQ(GetString(result.rows[1], 0), "Bob");
    EXPECT_FALSE(result.rows[2][0].has_value());
    EXPECT_EQ(GetString(result.rows[3], 0), "Carl");
    EXPECT_EQ(GetString(result.rows[4], 0), "Dasha");
}

TEST(ExecutorPlainSelect, AppliesWhereFilterOnFullScan) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::Less,
        MakeColumnExpr("age"),
        MakeIntLiteral(20)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 4u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 1);
    EXPECT_EQ(GetInt(result.rows[1], 0), 3);
    EXPECT_EQ(GetInt(result.rows[2], 0), 4);
    EXPECT_EQ(GetInt(result.rows[3], 0), 40);
}

TEST(ExecutorPlainSelect, UsesPointLookupForPureKeyPredicate) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::Equal,
        MakeColumnExpr("id"),
        MakeIntLiteral(40)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_EQ(GetString(result.rows[0], 0), "Dasha");
}

TEST(ExecutorPlainSelect, UsesPointLookupAndAppliesResidualWhere) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::And,
        MakeBinaryExpr(
            parser::BinaryOperation::Equal,
            MakeColumnExpr("id"),
            MakeIntLiteral(40)
        ),
        MakeBinaryExpr(
            parser::BinaryOperation::Less,
            MakeColumnExpr("age"),
            MakeIntLiteral(20)
        )
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 40);
}

TEST(ExecutorPlainSelect, PointLookupWithResidualWhereCanReturnEmpty) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::And,
        MakeBinaryExpr(
            parser::BinaryOperation::Equal,
            MakeColumnExpr("id"),
            MakeIntLiteral(2)
        ),
        MakeBinaryExpr(
            parser::BinaryOperation::Less,
            MakeColumnExpr("age"),
            MakeIntLiteral(20)
        )
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    EXPECT_TRUE(result.rows.empty());
}

TEST(ExecutorPlainSelect, UsesRangeScanForKeyInterval) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::And,
        MakeBinaryExpr(
            parser::BinaryOperation::GreaterEqual,
            MakeColumnExpr("id"),
            MakeIntLiteral(2)
        ),
        MakeBinaryExpr(
            parser::BinaryOperation::Less,
            MakeColumnExpr("id"),
            MakeIntLiteral(4)
        )
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 2u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 2);
    EXPECT_EQ(GetInt(result.rows[1], 0), 3);
}

TEST(ExecutorPlainSelect, UsesRangeScanAndAppliesResidualWhere) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::And,
        MakeBinaryExpr(
            parser::BinaryOperation::And,
            MakeBinaryExpr(
                parser::BinaryOperation::GreaterEqual,
                MakeColumnExpr("id"),
                MakeIntLiteral(1)
            ),
            MakeBinaryExpr(
                parser::BinaryOperation::LessEqual,
                MakeColumnExpr("id"),
                MakeIntLiteral(4)
            )
        ),
        MakeBinaryExpr(
            parser::BinaryOperation::Less,
            MakeColumnExpr("age"),
            MakeIntLiteral(20)
        )
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 3u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 1);
    EXPECT_EQ(GetInt(result.rows[1], 0), 3);
    EXPECT_EQ(GetInt(result.rows[2], 0), 4);
}

TEST(ExecutorPlainSelect, ReturnsEmptyForContradictingPointPredicates) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
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

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    EXPECT_TRUE(result.rows.empty());
}

TEST(ExecutorPlainSelect, AppliesLimit) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.limit = 2;

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 2u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 1);
    EXPECT_EQ(GetInt(result.rows[1], 0), 2);
}

TEST(ExecutorPlainSelect, EvaluatesIsNullExpression) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemExpr(MakeIsNullExpr(MakeColumnExpr("name"), false))
    );
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::Equal,
        MakeColumnExpr("id"),
        MakeIntLiteral(3)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 1u);
    EXPECT_TRUE(GetBool(result.rows[0], 0));
}

TEST(ExecutorPlainSelect, EvaluatesNotExpressionInWhere) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.where_expression = MakeUnaryExpr(
        parser::UnaryOperation::Not,
        MakeBinaryExpr(
            parser::BinaryOperation::GreaterEqual,
            MakeColumnExpr("age"),
            MakeIntLiteral(20)
        )
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 4u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 1);
    EXPECT_EQ(GetInt(result.rows[1], 0), 3);
    EXPECT_EQ(GetInt(result.rows[2], 0), 4);
    EXPECT_EQ(GetInt(result.rows[3], 0), 40);
}

TEST(ExecutorPlainSelect, OrdersByColumnAscending) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("age"), parser::OrderDirection::Asc)
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("id"), parser::OrderDirection::Asc)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 5u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 3);
    EXPECT_EQ(GetInt(result.rows[1], 0), 1);
    EXPECT_EQ(GetInt(result.rows[2], 0), 4);
    EXPECT_EQ(GetInt(result.rows[3], 0), 40);
    EXPECT_EQ(GetInt(result.rows[4], 0), 2);
}

TEST(ExecutorPlainSelect, OrdersByColumnDescending) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("age"), parser::OrderDirection::Desc)
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("id"), parser::OrderDirection::Asc)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 5u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 2);
    EXPECT_EQ(GetInt(result.rows[1], 0), 1);
    EXPECT_EQ(GetInt(result.rows[2], 0), 4);
    EXPECT_EQ(GetInt(result.rows[3], 0), 40);
    EXPECT_EQ(GetInt(result.rows[4], 0), 3);
}

TEST(ExecutorPlainSelect, OrdersByNonSelectedExpression) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("age"), parser::OrderDirection::Asc)
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("id"), parser::OrderDirection::Asc)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 5u);
    EXPECT_FALSE(result.rows[0][0].has_value());
    EXPECT_EQ(GetString(result.rows[1], 0), "Ann");
    EXPECT_EQ(GetString(result.rows[2], 0), "Carl");
    EXPECT_EQ(GetString(result.rows[3], 0), "Dasha");
    EXPECT_EQ(GetString(result.rows[4], 0), "Bob");
}

TEST(ExecutorPlainSelect, OrdersByColumnWithLimit) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("age"), parser::OrderDirection::Asc)
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("id"), parser::OrderDirection::Asc)
    );
    statement.limit = 2;

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 2u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 3);
    EXPECT_EQ(GetInt(result.rows[1], 0), 1);
}

TEST(ExecutorPlainSelect, OrdersNullsLastInAscendingOrder) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("salary"), parser::OrderDirection::Asc)
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("id"), parser::OrderDirection::Asc)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 5u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 1);
    EXPECT_EQ(GetInt(result.rows[1], 0), 4);
    EXPECT_EQ(GetInt(result.rows[2], 0), 3);
    EXPECT_EQ(GetInt(result.rows[3], 0), 40);
    EXPECT_EQ(GetInt(result.rows[4], 0), 2);
}

TEST(ExecutorPlainSelect, OrdersByMultipleColumnsWithMixedDirections) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("age"), parser::OrderDirection::Asc)
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("name"), parser::OrderDirection::Desc)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 5u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 3);
    EXPECT_EQ(GetInt(result.rows[1], 0), 40);
    EXPECT_EQ(GetInt(result.rows[2], 0), 4);
    EXPECT_EQ(GetInt(result.rows[3], 0), 1);
    EXPECT_EQ(GetInt(result.rows[4], 0), 2);
}

TEST(ExecutorPlainSelect, AppliesWhereThenOrdersByAnotherColumn) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::Less,
        MakeColumnExpr("age"),
        MakeIntLiteral(25)
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("salary"), parser::OrderDirection::Desc)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 4u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 40);
    EXPECT_EQ(GetInt(result.rows[1], 0), 3);
    EXPECT_EQ(GetInt(result.rows[2], 0), 4);
    EXPECT_EQ(GetInt(result.rows[3], 0), 1);
}

TEST(ExecutorPlainSelect, OrdersByBooleanExpression) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.order_by_items.push_back(
        MakeOrderByItem(
            MakeIsNullExpr(MakeColumnExpr("name"), false),
            parser::OrderDirection::Asc
        )
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("id"), parser::OrderDirection::Asc)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 5u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 1);
    EXPECT_EQ(GetInt(result.rows[1], 0), 2);
    EXPECT_EQ(GetInt(result.rows[2], 0), 4);
    EXPECT_EQ(GetInt(result.rows[3], 0), 40);
    EXPECT_EQ(GetInt(result.rows[4], 0), 3);
}

TEST(ExecutorPlainSelect, AppliesWhereOrderByAndLimitTogether) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::Equal,
        MakeColumnExpr("age"),
        MakeIntLiteral(19)
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("name"), parser::OrderDirection::Desc)
    );
    statement.limit = 2;

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 2u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 40);
    EXPECT_EQ(GetInt(result.rows[1], 0), 4);
}

TEST(ExecutorPlainSelect, UsesRangeScanResidualFilterAndOrderBy) {
    auto engine = MakeEngineWithUsersData();

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("id")));
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::And,
        MakeBinaryExpr(
            parser::BinaryOperation::And,
            MakeBinaryExpr(
                parser::BinaryOperation::GreaterEqual,
                MakeColumnExpr("id"),
                MakeIntLiteral(1)
            ),
            MakeBinaryExpr(
                parser::BinaryOperation::LessEqual,
                MakeColumnExpr("id"),
                MakeIntLiteral(40)
            )
        ),
        MakeBinaryExpr(
            parser::BinaryOperation::Less,
            MakeColumnExpr("age"),
            MakeIntLiteral(25)
        )
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("age"), parser::OrderDirection::Asc)
    );
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("id"), parser::OrderDirection::Desc)
    );

    executor::SelectResult result = ExecutePlainSelect(engine, statement);

    ASSERT_EQ(result.rows.size(), 4u);
    EXPECT_EQ(GetInt(result.rows[0], 0), 3);
    EXPECT_EQ(GetInt(result.rows[1], 0), 40);
    EXPECT_EQ(GetInt(result.rows[2], 0), 4);
    EXPECT_EQ(GetInt(result.rows[3], 0), 1);
}
