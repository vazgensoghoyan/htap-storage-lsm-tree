#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "parser/ast.hpp"
#include "parser/error.hpp"
#include "parser/lexer.hpp"
#include "parser/parser.hpp"

namespace htap::parser {
namespace {

std::unique_ptr<Statement> ParseSingleStatement(const std::string& query) {
    Lexer lexer(query);
    Parser parser(std::move(lexer));
    return parser.ParseStatement();
}

template <typename T>
T* As(Statement* statement) {
    return dynamic_cast<T*>(statement);
}

template <typename T>
T* As(Expression* expression) {
    return dynamic_cast<T*>(expression);
}

template <typename T>
T* As(SelectItem* item) {
    return dynamic_cast<T*>(item);
}

TEST(ParserTest, ParsesCreateTableStatement) {
    const std::string query =
        "CREATE TABLE users ("
        "key STRING KEY, "
        "name STRING NULL, "
        "age INT64, "
        "salary DOUBLE NULL"
        ");";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* create_statement = As<CreateTableStatement>(statement.get());
    ASSERT_NE(create_statement, nullptr);

    EXPECT_EQ(create_statement->table_name, "users");
    ASSERT_EQ(create_statement->columns.size(), 4u);

    EXPECT_EQ(create_statement->columns[0].name, "key");
    EXPECT_EQ(create_statement->columns[0].type, DataType::String);
    EXPECT_TRUE(create_statement->columns[0].is_key);
    EXPECT_FALSE(create_statement->columns[0].nullable);

    EXPECT_EQ(create_statement->columns[1].name, "name");
    EXPECT_EQ(create_statement->columns[1].type, DataType::String);
    EXPECT_FALSE(create_statement->columns[1].is_key);
    EXPECT_TRUE(create_statement->columns[1].nullable);

    EXPECT_EQ(create_statement->columns[2].name, "age");
    EXPECT_EQ(create_statement->columns[2].type, DataType::Int64);
    EXPECT_FALSE(create_statement->columns[2].nullable);

    EXPECT_EQ(create_statement->columns[3].name, "salary");
    EXPECT_EQ(create_statement->columns[3].type, DataType::Double);
    EXPECT_TRUE(create_statement->columns[3].nullable);
}

TEST(ParserTest, ParsesCreateTableWithNotNull) {
    const std::string query =
        "CREATE TABLE metrics ("
        "key STRING KEY, "
        "value DOUBLE NOT NULL"
        ");";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* create_statement = As<CreateTableStatement>(statement.get());
    ASSERT_NE(create_statement, nullptr);
    ASSERT_EQ(create_statement->columns.size(), 2u);

    EXPECT_EQ(create_statement->columns[1].name, "value");
    EXPECT_EQ(create_statement->columns[1].type, DataType::Double);
    EXPECT_FALSE(create_statement->columns[1].nullable);
}

TEST(ParserTest, ParsesInsertStatement) {
    const std::string query =
        "INSERT INTO users (key, name, age, salary) "
        "VALUES ('u1', 'Dasha', 19, -10.5);";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* insert_statement = As<InsertStatement>(statement.get());
    ASSERT_NE(insert_statement, nullptr);

    EXPECT_EQ(insert_statement->table_name, "users");
    ASSERT_EQ(insert_statement->column_names.size(), 4u);
    EXPECT_EQ(insert_statement->column_names[0], "key");
    EXPECT_EQ(insert_statement->column_names[1], "name");
    EXPECT_EQ(insert_statement->column_names[2], "age");
    EXPECT_EQ(insert_statement->column_names[3], "salary");

    ASSERT_EQ(insert_statement->values.size(), 4u);

    EXPECT_NE(As<StringLiteral>(insert_statement->values[0].get()), nullptr);
    EXPECT_NE(As<StringLiteral>(insert_statement->values[1].get()), nullptr);
    EXPECT_NE(As<IntLiteral>(insert_statement->values[2].get()), nullptr);
    EXPECT_NE(As<DoubleLiteral>(insert_statement->values[3].get()), nullptr);
}

TEST(ParserTest, BuildsExactAstForInsertValues) {
    const std::string query =
        "INSERT INTO users (key, age, salary, note) "
        "VALUES ('u1', -42, -10.5, NULL);";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* insert_statement = As<InsertStatement>(statement.get());
    ASSERT_NE(insert_statement, nullptr);
    ASSERT_EQ(insert_statement->values.size(), 4u);

    auto* value0 = As<StringLiteral>(insert_statement->values[0].get());
    ASSERT_NE(value0, nullptr);
    EXPECT_EQ(value0->value, "u1");

    auto* value1 = As<IntLiteral>(insert_statement->values[1].get());
    ASSERT_NE(value1, nullptr);
    EXPECT_EQ(value1->value, -42);

    auto* value2 = As<DoubleLiteral>(insert_statement->values[2].get());
    ASSERT_NE(value2, nullptr);
    EXPECT_DOUBLE_EQ(value2->value, -10.5);

    auto* value3 = As<NullLiteral>(insert_statement->values[3].get());
    ASSERT_NE(value3, nullptr);
}

TEST(ParserTest, ParsesSimpleSelectStatement) {
    const std::string query =
        "SELECT name, age "
        "FROM users;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);

    EXPECT_EQ(select_statement->table_name, "users");
    ASSERT_EQ(select_statement->select_items.size(), 2u);

    EXPECT_EQ(select_statement->where_expression, nullptr);
    EXPECT_TRUE(select_statement->group_by_items.empty());
    EXPECT_TRUE(select_statement->order_by_items.empty());
    EXPECT_FALSE(select_statement->limit.has_value());
}

TEST(ParserTest, ParsesSelectStar) {
    const std::string query = "SELECT * FROM users;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);
    ASSERT_EQ(select_statement->select_items.size(), 1u);

    EXPECT_NE(As<SelectItemStar>(select_statement->select_items[0].get()), nullptr);
}

TEST(ParserTest, BuildsExactAstForAggregateSelectItem) {
    const std::string query = "SELECT AVG(score) FROM users;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);
    ASSERT_EQ(select_statement->select_items.size(), 1u);

    auto* aggregate = As<SelectAggregateExpression>(select_statement->select_items[0].get());
    ASSERT_NE(aggregate, nullptr);
    EXPECT_EQ(aggregate->kind, AggregateKind::Avg);

    auto* argument = As<ColumnExpression>(aggregate->expression.get());
    ASSERT_NE(argument, nullptr);
    EXPECT_EQ(argument->column_name, "score");
}

TEST(ParserTest, ParsesSelectStatementWithClauses) {
    const std::string query =
        "SELECT city, COUNT(key) "
        "FROM users "
        "WHERE age >= 18 AND score > 10 "
        "GROUP BY city "
        "ORDER BY city DESC "
        "LIMIT 10;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);

    EXPECT_EQ(select_statement->table_name, "users");
    ASSERT_EQ(select_statement->select_items.size(), 2u);

    EXPECT_NE(select_statement->where_expression, nullptr);
    ASSERT_EQ(select_statement->group_by_items.size(), 1u);
    ASSERT_EQ(select_statement->order_by_items.size(), 1u);

    ASSERT_TRUE(select_statement->limit.has_value());
    EXPECT_EQ(select_statement->limit.value(), 10u);
    EXPECT_EQ(select_statement->order_by_items[0].direction, OrderDirection::Desc);
}

TEST(ParserTest, ParsesMultipleGroupByAndOrderByItems) {
    const std::string query =
        "SELECT city, country, COUNT(key) "
        "FROM users "
        "GROUP BY city, country "
        "ORDER BY city ASC, country DESC "
        "LIMIT 5;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);

    ASSERT_EQ(select_statement->group_by_items.size(), 2u);
    ASSERT_EQ(select_statement->order_by_items.size(), 2u);

    EXPECT_EQ(select_statement->order_by_items[0].direction, OrderDirection::Asc);
    EXPECT_EQ(select_statement->order_by_items[1].direction, OrderDirection::Desc);

    ASSERT_TRUE(select_statement->limit.has_value());
    EXPECT_EQ(select_statement->limit.value(), 5u);
}

TEST(ParserTest, BuildsExactAstForSimpleWhereComparison) {
    const std::string query =
        "SELECT name FROM users WHERE age >= 18;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);
    ASSERT_NE(select_statement->where_expression, nullptr);

    auto* root = As<BinaryExpression>(select_statement->where_expression.get());
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->operation, BinaryOperation::GreaterEqual);

    auto* left = As<ColumnExpression>(root->left.get());
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->column_name, "age");

    auto* right = As<IntLiteral>(root->right.get());
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->value, 18);
}

TEST(ParserTest, BuildsExactAstForIsNull) {
    const std::string query =
        "SELECT city FROM users WHERE city IS NULL;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);

    auto* is_null = As<IsNullExpression>(select_statement->where_expression.get());
    ASSERT_NE(is_null, nullptr);
    EXPECT_FALSE(is_null->is_not);

    auto* inner = As<ColumnExpression>(is_null->expression.get());
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->column_name, "city");
}

TEST(ParserTest, BuildsExactAstForIsNotNull) {
    const std::string query =
        "SELECT city FROM users WHERE city IS NOT NULL;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);

    auto* is_null = As<IsNullExpression>(select_statement->where_expression.get());
    ASSERT_NE(is_null, nullptr);
    EXPECT_TRUE(is_null->is_not);

    auto* inner = As<ColumnExpression>(is_null->expression.get());
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->column_name, "city");
}

TEST(ParserTest, BuildsExactAstForAndOrPrecedence) {
    const std::string query =
        "SELECT name FROM users WHERE a = 1 AND b = 2 OR c = 3;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);

    auto* root = As<BinaryExpression>(select_statement->where_expression.get());
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->operation, BinaryOperation::Or);

    auto* left_and = As<BinaryExpression>(root->left.get());
    ASSERT_NE(left_and, nullptr);
    EXPECT_EQ(left_and->operation, BinaryOperation::And);

    auto* right_eq = As<BinaryExpression>(root->right.get());
    ASSERT_NE(right_eq, nullptr);
    EXPECT_EQ(right_eq->operation, BinaryOperation::Equal);

    auto* right_eq_left = As<ColumnExpression>(right_eq->left.get());
    ASSERT_NE(right_eq_left, nullptr);
    EXPECT_EQ(right_eq_left->column_name, "c");

    auto* right_eq_right = As<IntLiteral>(right_eq->right.get());
    ASSERT_NE(right_eq_right, nullptr);
    EXPECT_EQ(right_eq_right->value, 3);

    auto* left_eq = As<BinaryExpression>(left_and->left.get());
    ASSERT_NE(left_eq, nullptr);
    EXPECT_EQ(left_eq->operation, BinaryOperation::Equal);

    auto* left_eq_left = As<ColumnExpression>(left_eq->left.get());
    ASSERT_NE(left_eq_left, nullptr);
    EXPECT_EQ(left_eq_left->column_name, "a");

    auto* left_eq_right = As<IntLiteral>(left_eq->right.get());
    ASSERT_NE(left_eq_right, nullptr);
    EXPECT_EQ(left_eq_right->value, 1);

    auto* middle_eq = As<BinaryExpression>(left_and->right.get());
    ASSERT_NE(middle_eq, nullptr);
    EXPECT_EQ(middle_eq->operation, BinaryOperation::Equal);

    auto* middle_eq_left = As<ColumnExpression>(middle_eq->left.get());
    ASSERT_NE(middle_eq_left, nullptr);
    EXPECT_EQ(middle_eq_left->column_name, "b");

    auto* middle_eq_right = As<IntLiteral>(middle_eq->right.get());
    ASSERT_NE(middle_eq_right, nullptr);
    EXPECT_EQ(middle_eq_right->value, 2);
}

TEST(ParserTest, BuildsExactAstForNotExpression) {
    const std::string query =
        "SELECT name FROM users WHERE NOT active;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);

    auto* unary = As<UnaryExpression>(select_statement->where_expression.get());
    ASSERT_NE(unary, nullptr);
    EXPECT_EQ(unary->operation, UnaryOperation::Not);

    auto* operand = As<ColumnExpression>(unary->expression.get());
    ASSERT_NE(operand, nullptr);
    EXPECT_EQ(operand->column_name, "active");
}

TEST(ParserTest, BuildsExactAstForParenthesizedExpression) {
    const std::string query =
        "SELECT name FROM users WHERE (a = 1 OR b = 2) AND c = 3;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);

    auto* root = As<BinaryExpression>(select_statement->where_expression.get());
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->operation, BinaryOperation::And);

    auto* left = As<BinaryExpression>(root->left.get());
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->operation, BinaryOperation::Or);

    auto* right = As<BinaryExpression>(root->right.get());
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->operation, BinaryOperation::Equal);
}

TEST(ParserTest, ThrowsOnInvalidSelectStatement) {
    const std::string query = "SELECT FROM users;";

    EXPECT_THROW(
        {
            std::unique_ptr<Statement> statement = ParseSingleStatement(query);
        },
        ParseError
    );
}

TEST(ParserTest, ThrowsOnMissingTableNameInCreateTable) {
    const std::string query = "CREATE TABLE (key STRING KEY);";

    EXPECT_THROW(
        {
            std::unique_ptr<Statement> statement = ParseSingleStatement(query);
        },
        ParseError
    );
}

TEST(ParserTest, ThrowsOnMissingValuesKeyword) {
    const std::string query =
        "INSERT INTO users (key, age) ('u1', 18);";

    EXPECT_THROW(
        {
            std::unique_ptr<Statement> statement = ParseSingleStatement(query);
        },
        ParseError
    );
}

TEST(ParserTest, ThrowsOnMissingByAfterGroup) {
    const std::string query =
        "SELECT city FROM users GROUP city;";

    EXPECT_THROW(
        {
            std::unique_ptr<Statement> statement = ParseSingleStatement(query);
        },
        ParseError
    );
}

TEST(ParserTest, ThrowsOnMissingByAfterOrder) {
    const std::string query =
        "SELECT city FROM users ORDER city;";

    EXPECT_THROW(
        {
            std::unique_ptr<Statement> statement = ParseSingleStatement(query);
        },
        ParseError
    );
}

TEST(ParserTest, ThrowsOnMissingSemicolon) {
    const std::string query =
        "SELECT name FROM users";

    EXPECT_THROW(
        {
            std::unique_ptr<Statement> statement = ParseSingleStatement(query);
        },
        ParseError
    );
}

TEST(ParserTest, BuildsExactAstForGroupByAndOrderByExpressions) {
    const std::string query =
        "SELECT city, COUNT(key) "
        "FROM users "
        "GROUP BY city "
        "ORDER BY city DESC;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);

    ASSERT_EQ(select_statement->group_by_items.size(), 1u);
    auto* group_by_expression = As<ColumnExpression>(select_statement->group_by_items[0].get());
    ASSERT_NE(group_by_expression, nullptr);
    EXPECT_EQ(group_by_expression->column_name, "city");

    ASSERT_EQ(select_statement->order_by_items.size(), 1u);
    auto* order_by_expression = As<ColumnExpression>(select_statement->order_by_items[0].expression.get());
    ASSERT_NE(order_by_expression, nullptr);
    EXPECT_EQ(order_by_expression->column_name, "city");
    EXPECT_EQ(select_statement->order_by_items[0].direction, OrderDirection::Desc);
}

TEST(ParserTest, BuildsExactAstForMixedSelectItems) {
    const std::string query =
        "SELECT city, age, AVG(score) "
        "FROM users;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);
    ASSERT_EQ(select_statement->select_items.size(), 3u);

    auto* item0 = As<SelectItemExpression>(select_statement->select_items[0].get());
    ASSERT_NE(item0, nullptr);
    auto* item0_expression = As<ColumnExpression>(item0->expression.get());
    ASSERT_NE(item0_expression, nullptr);
    EXPECT_EQ(item0_expression->column_name, "city");

    auto* item1 = As<SelectItemExpression>(select_statement->select_items[1].get());
    ASSERT_NE(item1, nullptr);
    auto* item1_expression = As<ColumnExpression>(item1->expression.get());
    ASSERT_NE(item1_expression, nullptr);
    EXPECT_EQ(item1_expression->column_name, "age");

    auto* item2 = As<SelectAggregateExpression>(select_statement->select_items[2].get());
    ASSERT_NE(item2, nullptr);
    EXPECT_EQ(item2->kind, AggregateKind::Avg);

    auto* item2_expression = As<ColumnExpression>(item2->expression.get());
    ASSERT_NE(item2_expression, nullptr);
    EXPECT_EQ(item2_expression->column_name, "score");
}

TEST(ParserTest, ThrowsOnMissingRightSideOfComparison) {
    const std::string query =
        "SELECT name FROM users WHERE a = ;";

    EXPECT_THROW(
        {
            std::unique_ptr<Statement> statement = ParseSingleStatement(query);
        },
        ParseError
    );
}

TEST(ParserTest, ThrowsOnMissingClosingParenthesisInWhere) {
    const std::string query =
        "SELECT name FROM users WHERE (a = 1;";

    EXPECT_THROW(
        {
            std::unique_ptr<Statement> statement = ParseSingleStatement(query);
        },
        ParseError
    );
}

TEST(ParserTest, ThrowsOnIncompleteIsNotNullExpression) {
    const std::string query =
        "SELECT city FROM users WHERE city IS NOT;";

    EXPECT_THROW(
        {
            std::unique_ptr<Statement> statement = ParseSingleStatement(query);
        },
        ParseError
    );
}

TEST(ParserTest, UsesAscendingOrderByDirectionByDefault) {
    const std::string query =
        "SELECT city FROM users ORDER BY city;";

    std::unique_ptr<Statement> statement = ParseSingleStatement(query);

    auto* select_statement = As<SelectStatement>(statement.get());
    ASSERT_NE(select_statement, nullptr);
    ASSERT_EQ(select_statement->order_by_items.size(), 1u);

    auto* order_by_expression = As<ColumnExpression>(select_statement->order_by_items[0].expression.get());
    ASSERT_NE(order_by_expression, nullptr);
    EXPECT_EQ(order_by_expression->column_name, "city");

    EXPECT_EQ(select_statement->order_by_items[0].direction, OrderDirection::Asc);
}

} 
}