#pragma once

#include <memory>
#include <utility>
#include <cstddef>

#include "storage/api/types.hpp"
#include "parser/ast.hpp"

namespace htap::executor {

enum class ExpressionType {
    String,
    Int64,
    Double,
    Boolean,
    Null
};

enum class BoundExpressionKind {
    Literal,
    Column,
    Unary,
    Binary,
    IsNull
};

constexpr ExpressionType ValueTypeToExpressionType(storage::ValueType type) {
    switch (type) {
        case storage::ValueType::INT64:
            return ExpressionType::Int64;
        case storage::ValueType::DOUBLE:
            return ExpressionType::Double;
        case storage::ValueType::STRING:
            return ExpressionType::String;
    }

    return ExpressionType::Null;
}

struct BoundExpression {
    explicit BoundExpression(BoundExpressionKind kind, ExpressionType type)
        : kind(kind),
        type(type) {
    }

    virtual ~BoundExpression() = default;

    BoundExpressionKind kind;
    ExpressionType type;
};

struct BoundLiteralExpression : BoundExpression {
    BoundLiteralExpression(storage::NullableValue value, ExpressionType type)
        : BoundExpression(BoundExpressionKind::Literal, type),
        value(std::move(value)) {
    }

    storage::NullableValue value;
};

struct BoundColumnExpression : BoundExpression {
    BoundColumnExpression(std::size_t column_index, storage::ValueType type)
        : BoundExpression(BoundExpressionKind::Column, ValueTypeToExpressionType(type)),
        column_index(column_index) {
    }

    std::size_t column_index;
};

struct BoundUnaryExpression : BoundExpression {
    BoundUnaryExpression(
        parser::UnaryOperation operation,
        std::unique_ptr<BoundExpression> expression)
        : BoundExpression(BoundExpressionKind::Unary, ExpressionType::Boolean),
        operation(operation),
        expression(std::move(expression)) {
    }

    parser::UnaryOperation operation;
    std::unique_ptr<BoundExpression> expression;
};

struct BoundBinaryExpression : BoundExpression {
    BoundBinaryExpression(
        parser::BinaryOperation operation,
        std::unique_ptr<BoundExpression> left,
        std::unique_ptr<BoundExpression> right)
        : BoundExpression(BoundExpressionKind::Binary, ExpressionType::Boolean),
        operation(operation),
        left(std::move(left)),
        right(std::move(right)) {
    }

    parser::BinaryOperation operation;
    std::unique_ptr<BoundExpression> left;
    std::unique_ptr<BoundExpression> right;
};

struct BoundIsNullExpression : BoundExpression {
    BoundIsNullExpression(
        std::unique_ptr<BoundExpression> expression,
        bool is_not)
        : BoundExpression(BoundExpressionKind::IsNull, ExpressionType::Boolean),
        expression(std::move(expression)),
        is_not(is_not) {
    }

    std::unique_ptr<BoundExpression> expression;
    bool is_not;
};

struct BoundSelectItem {
    virtual ~BoundSelectItem() = default;
};

struct BoundSelectItemExpression : BoundSelectItem {
    explicit BoundSelectItemExpression(std::unique_ptr<BoundExpression> expression)
        : expression(std::move(expression)) {
        }

    std::unique_ptr<BoundExpression> expression;
};

struct BoundSelectAggregateExpression : BoundSelectItem {
    BoundSelectAggregateExpression(
        parser::AggregateKind kind,
        std::unique_ptr<BoundExpression> expression,
        ExpressionType result_type)
        : kind(kind),
        expression(std::move(expression)),
        result_type(result_type) {
        }

    parser::AggregateKind kind;
    std::unique_ptr<BoundExpression> expression;
    ExpressionType result_type;
};

struct BoundOrderByItem {
    std::unique_ptr<BoundExpression> expression;
    parser::OrderDirection direction = parser::OrderDirection::Asc;
};

}