#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "executor/binder.hpp"
#include "executor/error.hpp"
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

std::unique_ptr<parser::Expression> MakeDoubleLiteral(double value) {
    auto expr = std::make_unique<parser::DoubleLiteral>();
    expr->value = value;
    return expr;
}

std::unique_ptr<parser::Expression> MakeStringLiteral(const std::string& value) {
    auto expr = std::make_unique<parser::StringLiteral>();
    expr->value = value;
    return expr;
}

std::unique_ptr<parser::Expression> MakeNullLiteral() {
    return std::make_unique<parser::NullLiteral>();
}

std::unique_ptr<parser::Expression> MakeUnaryExpr(
    parser::UnaryOperation op,
    std::unique_ptr<parser::Expression> inner) {
    auto expr = std::make_unique<parser::UnaryExpression>();
    expr->operation = op;
    expr->expression = std::move(inner);
    return expr;
}

std::unique_ptr<parser::Expression> MakeBinaryExpr(
    parser::BinaryOperation op,
    std::unique_ptr<parser::Expression> left,
    std::unique_ptr<parser::Expression> right) {
    auto expr = std::make_unique<parser::BinaryExpression>();
    expr->operation = op;
    expr->left = std::move(left);
    expr->right = std::move(right);
    return expr;
}

std::unique_ptr<parser::Expression> MakeIsNullExpr(
    std::unique_ptr<parser::Expression> inner,
    bool is_not) {
    auto expr = std::make_unique<parser::IsNullExpression>();
    expr->expression = std::move(inner);
    expr->is_not = is_not;
    return expr;
}

std::unique_ptr<parser::SelectItem> MakeSelectItemExpr(
    std::unique_ptr<parser::Expression> expr) {
    auto item = std::make_unique<parser::SelectItemExpression>();
    item->expression = std::move(expr);
    return item;
}

std::unique_ptr<parser::SelectItem> MakeSelectItemAggregate(
    parser::AggregateKind kind,
    std::unique_ptr<parser::Expression> expr) {
    auto item = std::make_unique<parser::SelectAggregateExpression>();
    item->kind = kind;
    item->expression = std::move(expr);
    return item;
}

std::unique_ptr<parser::SelectItem> MakeSelectItemStar() {
    return std::make_unique<parser::SelectItemStar>();
}

parser::OrderByItem MakeOrderByItem(
    std::unique_ptr<parser::Expression> expr,
    parser::OrderDirection direction = parser::OrderDirection::Asc) {
    parser::OrderByItem item;
    item.expression = std::move(expr);
    item.direction = direction;
    return item;
}

} // namespace

// -------------------- CREATE TABLE --------------------

TEST(BinderCreateTable, BindsValidCreateTableStatement) {
    storage::MockStorageEngine engine;
    executor::Binder binder(engine);

    parser::CreateTableStatement statement;
    statement.table_name = "employees";
    statement.columns = {
        {"id", parser::DataType::Int64, false, true},
        {"name", parser::DataType::String, true, false},
        {"salary", parser::DataType::Double, true, false}
    };

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* create = dynamic_cast<executor::BoundCreateTableStatement*>(bound.get());

    ASSERT_NE(create, nullptr);
    EXPECT_EQ(create->table_name, "employees");
    EXPECT_EQ(create->schema.size(), 3u);
    EXPECT_EQ(create->schema.key_column_index(), 0u);
    EXPECT_EQ(create->schema.get_column(0).name, "id");
    EXPECT_EQ(create->schema.get_column(0).type, storage::ValueType::INT64);
    EXPECT_TRUE(create->schema.get_column(0).is_key);
    EXPECT_FALSE(create->schema.get_column(0).nullable);
}

TEST(BinderCreateTable, ThrowsOnDuplicateColumns) {
    storage::MockStorageEngine engine;
    executor::Binder binder(engine);

    parser::CreateTableStatement statement;
    statement.table_name = "employees";
    statement.columns = {
        {"id", parser::DataType::Int64, false, true},
        {"id", parser::DataType::String, true, false}
    };

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

TEST(BinderCreateTable, ThrowsWhenNoKeyColumn) {
    storage::MockStorageEngine engine;
    executor::Binder binder(engine);

    parser::CreateTableStatement statement;
    statement.table_name = "employees";
    statement.columns = {
        {"name", parser::DataType::String, true, false},
        {"age", parser::DataType::Int64, false, false}
    };

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

TEST(BinderCreateTable, ThrowsWhenKeyIsNotInt64) {
    storage::MockStorageEngine engine;
    executor::Binder binder(engine);

    parser::CreateTableStatement statement;
    statement.table_name = "employees";
    statement.columns = {
        {"id", parser::DataType::String, false, true},
        {"name", parser::DataType::String, true, false}
    };

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

TEST(BinderCreateTable, ThrowsWhenKeyIsNullable) {
    storage::MockStorageEngine engine;
    executor::Binder binder(engine);

    parser::CreateTableStatement statement;
    statement.table_name = "employees";
    statement.columns = {
        {"id", parser::DataType::Int64, true, true},
        {"name", parser::DataType::String, true, false}
    };

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

// -------------------- INSERT --------------------

TEST(BinderInsert, BindsValidInsertStatement) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"id", "name", "age", "salary"};
    statement.values.push_back(MakeIntLiteral(1));
    statement.values.push_back(MakeStringLiteral("Dasha"));
    statement.values.push_back(MakeIntLiteral(19));
    statement.values.push_back(MakeDoubleLiteral(1000.5));

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* insert = dynamic_cast<executor::BoundInsertStatement*>(bound.get());

    ASSERT_NE(insert, nullptr);
    ASSERT_NE(insert->schema, nullptr);
    ASSERT_EQ(insert->row_values.size(), 4u);

    ASSERT_TRUE(insert->row_values[0].has_value());
    ASSERT_TRUE(insert->row_values[1].has_value());
    ASSERT_TRUE(insert->row_values[2].has_value());
    ASSERT_TRUE(insert->row_values[3].has_value());

    EXPECT_EQ(std::get<std::int64_t>(*insert->row_values[0]), 1);
    EXPECT_EQ(std::get<std::string>(*insert->row_values[1]), "Dasha");
    EXPECT_EQ(std::get<std::int64_t>(*insert->row_values[2]), 19);
    EXPECT_DOUBLE_EQ(std::get<double>(*insert->row_values[3]), 1000.5);
}

TEST(BinderInsert, ReordersValuesIntoSchemaOrder) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"salary", "age", "name", "id"};
    statement.values.push_back(MakeDoubleLiteral(5000.0));
    statement.values.push_back(MakeIntLiteral(20));
    statement.values.push_back(MakeStringLiteral("Ann"));
    statement.values.push_back(MakeIntLiteral(7));

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* insert = dynamic_cast<executor::BoundInsertStatement*>(bound.get());

    ASSERT_NE(insert, nullptr);

    EXPECT_EQ(std::get<std::int64_t>(*insert->row_values[0]), 7);
    EXPECT_EQ(std::get<std::string>(*insert->row_values[1]), "Ann");
    EXPECT_EQ(std::get<std::int64_t>(*insert->row_values[2]), 20);
    EXPECT_DOUBLE_EQ(std::get<double>(*insert->row_values[3]), 5000.0);
}

TEST(BinderInsert, AllowsMissingNullableColumns) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"id", "age"};
    statement.values.push_back(MakeIntLiteral(10));
    statement.values.push_back(MakeIntLiteral(42));

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* insert = dynamic_cast<executor::BoundInsertStatement*>(bound.get());

    ASSERT_NE(insert, nullptr);
    EXPECT_EQ(std::get<std::int64_t>(*insert->row_values[0]), 10);
    EXPECT_FALSE(insert->row_values[1].has_value());
    EXPECT_EQ(std::get<std::int64_t>(*insert->row_values[2]), 42);
    EXPECT_FALSE(insert->row_values[3].has_value());
}

TEST(BinderInsert, ThrowsWhenMissingNonNullableColumn) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"id", "name"};
    statement.values.push_back(MakeIntLiteral(10));
    statement.values.push_back(MakeStringLiteral("Ann"));

    EXPECT_THROW(binder.Bind(statement), executor::TypeMismatchError);
}

TEST(BinderInsert, ThrowsWhenTableDoesNotExist) {
    storage::MockStorageEngine engine;
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"id"};
    statement.values.push_back(MakeIntLiteral(1));

    EXPECT_THROW(binder.Bind(statement), executor::TableNotFoundError);
}

TEST(BinderInsert, ThrowsWhenColumnDoesNotExist) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"id", "unknown", "age"};
    statement.values.push_back(MakeIntLiteral(1));
    statement.values.push_back(MakeStringLiteral("x"));
    statement.values.push_back(MakeIntLiteral(19));

    EXPECT_THROW(binder.Bind(statement), executor::ColumnNotFoundError);
}

TEST(BinderInsert, ThrowsWhenColumnListAndValuesCountDiffer) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"id", "age"};
    statement.values.push_back(MakeIntLiteral(1));

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

TEST(BinderInsert, ThrowsOnDuplicateInsertColumn) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"id", "id", "age"};
    statement.values.push_back(MakeIntLiteral(1));
    statement.values.push_back(MakeIntLiteral(2));
    statement.values.push_back(MakeIntLiteral(19));

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

TEST(BinderInsert, ThrowsWhenValueTypeDoesNotMatchSchema) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"id", "name", "age"};
    statement.values.push_back(MakeIntLiteral(1));
    statement.values.push_back(MakeStringLiteral("Ann"));
    statement.values.push_back(MakeStringLiteral("not_int"));

    EXPECT_THROW(binder.Bind(statement), executor::TypeMismatchError);
}

TEST(BinderInsert, ThrowsWhenKeyIsNull) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"id", "age"};
    statement.values.push_back(MakeNullLiteral());
    statement.values.push_back(MakeIntLiteral(19));

    EXPECT_THROW(binder.Bind(statement), executor::TypeMismatchError);
}

TEST(BinderInsert, ThrowsWhenInsertValueIsNotLiteral) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::InsertStatement statement;
    statement.table_name = "users";
    statement.column_names = {"id", "age"};
    statement.values.push_back(MakeIntLiteral(1));
    statement.values.push_back(MakeColumnExpr("id"));

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

// -------------------- SELECT --------------------

TEST(BinderSelect, BindsSimpleSelectColumn) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* select = dynamic_cast<executor::BoundSelectStatement*>(bound.get());

    ASSERT_NE(select, nullptr);
    ASSERT_NE(select->schema, nullptr);
    ASSERT_EQ(select->select_items.size(), 1u);

    auto* item =
        dynamic_cast<executor::BoundSelectItemExpression*>(select->select_items[0].get());
    ASSERT_NE(item, nullptr);

    auto* expr =
        dynamic_cast<executor::BoundColumnExpression*>(item->expression.get());
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(expr->column_index, 1u);
}

TEST(BinderSelect, ExpandsSelectStar) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemStar());

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* select = dynamic_cast<executor::BoundSelectStatement*>(bound.get());

    ASSERT_NE(select, nullptr);
    ASSERT_EQ(select->select_items.size(), 4u);

    for (std::size_t i = 0; i < select->select_items.size(); ++i) {
        auto* item =
            dynamic_cast<executor::BoundSelectItemExpression*>(select->select_items[i].get());
        ASSERT_NE(item, nullptr);

        auto* expr =
            dynamic_cast<executor::BoundColumnExpression*>(item->expression.get());
        ASSERT_NE(expr, nullptr);
        EXPECT_EQ(expr->column_index, i);
    }
}

TEST(BinderSelect, BindsWhereExpression) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));
    statement.where_expression = MakeBinaryExpr(
        parser::BinaryOperation::GreaterEqual,
        MakeColumnExpr("age"),
        MakeIntLiteral(18)
    );

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* select = dynamic_cast<executor::BoundSelectStatement*>(bound.get());

    ASSERT_NE(select, nullptr);
    ASSERT_NE(select->where_expression, nullptr);
    EXPECT_EQ(select->where_expression->type, executor::ExpressionType::Boolean);

    auto* where =
        dynamic_cast<executor::BoundBinaryExpression*>(select->where_expression.get());
    ASSERT_NE(where, nullptr);
    EXPECT_EQ(where->operation, parser::BinaryOperation::GreaterEqual);
}

TEST(BinderSelect, ThrowsWhenWhereIsNotBoolean) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));
    statement.where_expression = MakeColumnExpr("age");

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

TEST(BinderSelect, BindsOrderByItems) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));
    statement.order_by_items.push_back(
        MakeOrderByItem(MakeColumnExpr("age"), parser::OrderDirection::Desc)
    );

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* select = dynamic_cast<executor::BoundSelectStatement*>(bound.get());

    ASSERT_NE(select, nullptr);
    ASSERT_EQ(select->order_by_items.size(), 1u);
    EXPECT_EQ(select->order_by_items[0].direction, parser::OrderDirection::Desc);

    auto* expr =
        dynamic_cast<executor::BoundColumnExpression*>(select->order_by_items[0].expression.get());
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(expr->column_index, 2u);
}

TEST(BinderSelect, PreservesLimit) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));
    statement.limit = 10;

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* select = dynamic_cast<executor::BoundSelectStatement*>(bound.get());

    ASSERT_NE(select, nullptr);
    ASSERT_TRUE(select->limit.has_value());
    EXPECT_EQ(*select->limit, 10u);
}

TEST(BinderSelect, BindsCountAggregate) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* select = dynamic_cast<executor::BoundSelectStatement*>(bound.get());

    ASSERT_NE(select, nullptr);
    ASSERT_EQ(select->select_items.size(), 1u);

    auto* item =
        dynamic_cast<executor::BoundSelectAggregateExpression*>(select->select_items[0].get());
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->kind, parser::AggregateKind::Count);
    EXPECT_EQ(item->result_type, executor::ExpressionType::Int64);
}

TEST(BinderSelect, BindsAvgAggregateAsDouble) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Avg, MakeColumnExpr("age"))
    );

    std::unique_ptr<executor::BoundStatement> bound = binder.Bind(statement);
    auto* select = dynamic_cast<executor::BoundSelectStatement*>(bound.get());

    ASSERT_NE(select, nullptr);

    auto* item =
        dynamic_cast<executor::BoundSelectAggregateExpression*>(select->select_items[0].get());
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->result_type, executor::ExpressionType::Double);
}

TEST(BinderSelect, ThrowsOnStringAggregateArgumentForSum) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Sum, MakeColumnExpr("name"))
    );

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

TEST(BinderSelect, AllowsAggregatesWithoutGroupByWhenAllItemsAreAggregates) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Avg, MakeColumnExpr("age"))
    );

    EXPECT_NO_THROW(binder.Bind(statement));
}

TEST(BinderSelect, ThrowsWhenAggregateAndPlainItemWithoutGroupBy) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

TEST(BinderSelect, AllowsGroupedSelectWhenPlainItemAppearsInGroupBy) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.group_by_items.push_back(MakeColumnExpr("name"));

    EXPECT_NO_THROW(binder.Bind(statement));
}

TEST(BinderSelect, AllowsGroupedExpressionWhenExpressionMatchesGroupBy) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    auto expr1 = MakeBinaryExpr(
        parser::BinaryOperation::GreaterEqual,
        MakeColumnExpr("age"),
        MakeIntLiteral(18)
    );

    auto expr2 = MakeBinaryExpr(
        parser::BinaryOperation::GreaterEqual,
        MakeColumnExpr("age"),
        MakeIntLiteral(18)
    );

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(std::move(expr1)));
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.group_by_items.push_back(std::move(expr2));

    EXPECT_NO_THROW(binder.Bind(statement));
}

TEST(BinderSelect, ThrowsWhenPlainItemIsNotInGroupBy) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.group_by_items.push_back(MakeColumnExpr("age"));

    EXPECT_THROW(binder.Bind(statement), executor::InvalidQueryError);
}

TEST(BinderSelect, ThrowsWhenSelectColumnDoesNotExist) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("unknown")));

    EXPECT_THROW(binder.Bind(statement), executor::ColumnNotFoundError);
}

TEST(BinderSelect, ThrowsWhenGroupByColumnDoesNotExist) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(
        MakeSelectItemAggregate(parser::AggregateKind::Count, MakeColumnExpr("id"))
    );
    statement.group_by_items.push_back(MakeColumnExpr("unknown"));

    EXPECT_THROW(binder.Bind(statement), executor::ColumnNotFoundError);
}

TEST(BinderSelect, ThrowsWhenOrderByColumnDoesNotExist) {
    auto engine = MakeEngineWithUsers();
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));
    statement.order_by_items.push_back(MakeOrderByItem(MakeColumnExpr("unknown")));

    EXPECT_THROW(binder.Bind(statement), executor::ColumnNotFoundError);
}

TEST(BinderSelect, ThrowsWhenTableDoesNotExist) {
    storage::MockStorageEngine engine;
    executor::Binder binder(engine);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(MakeSelectItemExpr(MakeColumnExpr("name")));

    EXPECT_THROW(binder.Bind(statement), executor::TableNotFoundError);
}
