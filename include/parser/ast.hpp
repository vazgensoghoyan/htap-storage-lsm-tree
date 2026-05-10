#pragma once 

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace htap::parser {

enum class OrderDirection {
    Asc,
    Desc
};

enum class DataType {
    String,
    Int64,
    Double
};

enum class AggregateKind {
    Count,
    Sum,
    Avg,
    Min,
    Max
};

enum class UnaryOperation {
    Not
};

enum class BinaryOperation {
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    And,
    Or
};

struct ColumnDefinition {
    std::string name;
    DataType type;
    bool nullable = false;
    bool is_key = false;
};

struct Expression {
    virtual ~Expression() = default;
};

struct LiteralExpression : Expression {
    virtual ~LiteralExpression() = default;
};

struct NullLiteral : LiteralExpression {
};

struct IntLiteral : LiteralExpression {
    std::int64_t value;
};

struct DoubleLiteral : LiteralExpression {
    double value;
};

struct StringLiteral : LiteralExpression {
    std::string value;
};

struct UnaryExpression : Expression {
    UnaryOperation operation;
    std::unique_ptr<Expression> expression;
};

struct BinaryExpression : Expression {
    BinaryOperation operation;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct ColumnExpression : Expression {
    std::string column_name;
};

struct IsNullExpression : Expression {
    std::unique_ptr<Expression> expression;
    bool is_not;
};

struct SelectItem {
    virtual ~SelectItem() = default;
};

struct SelectItemStar : SelectItem {};

struct SelectItemExpression : SelectItem {
    std::unique_ptr<Expression> expression;
};

struct SelectAggregateExpression : SelectItem {
    AggregateKind kind;
    std::unique_ptr<Expression> expression;
};

struct OrderByItem {
    std::unique_ptr<Expression> expression;
    OrderDirection direction = OrderDirection::Asc;
};  

struct Statement {
    virtual ~Statement() = default;
};

struct CreateTableStatement : Statement {
    std::string table_name;
    std::vector<ColumnDefinition> columns;
};

struct InsertStatement : Statement {
    std::string table_name;
    std::vector<std::string> column_names;
    std::vector<std::unique_ptr<Expression>> values;
};

struct SelectStatement : Statement {
    std::string table_name;
    std::vector<std::unique_ptr<SelectItem>> select_items;
    std::unique_ptr<Expression> where_expression;
    std::vector<std::unique_ptr<Expression>> group_by_items;
    std::vector<OrderByItem> order_by_items;
    std::optional<std::size_t> limit;
};

}
