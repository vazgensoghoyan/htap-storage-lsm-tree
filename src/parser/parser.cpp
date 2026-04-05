#include "parser/parser.hpp"

#include <utility>

namespace htap::parser {

Parser::Parser(Lexer lexer): lexer_(std::move(lexer)) {
    Advance();
}

std::unique_ptr<Statement> Parser::ParseStatement() {
    if (IsExpectedToken(TokenType::Create)) {
        return ParseCreateTableStatement();
    }

    if (IsExpectedToken(TokenType::Insert)) {
        return ParseInsertStatement();
    }

    if (IsExpectedToken(TokenType::Select)) {
        return ParseSelectStatement();
    }

    throw ParseError("Expected CREATE, INSERT or SELECT statement", current_.position);
}

std::unique_ptr<CreateTableStatement> Parser::ParseCreateTableStatement() {
    if (!IsExpectedToken(TokenType::Create)) {
        throw ParseError("Expected CREATE", current_.position);
    }
    Advance();

    if (!IsExpectedToken(TokenType::Table)) {
        throw ParseError("Expected TABLE after CREATE", current_.position);
    }
    Advance();

    if (!IsExpectedToken(TokenType::Identifier)) {
        throw ParseError("Expected table name", current_.position);
    }

    auto statement = std::make_unique<CreateTableStatement>();
    statement->table_name = current_.text;
    Advance();

    if (!IsExpectedToken(TokenType::LParen)) {
        throw ParseError("Expected '(' after table name", current_.position);
    }
    Advance();

    statement->columns.push_back(ParseColumnDefinition());

    while (IsExpectedToken(TokenType::Comma)) {
        Advance();
        statement->columns.push_back(ParseColumnDefinition());
    }

    if (!IsExpectedToken(TokenType::RParen)) {
        throw ParseError("Expected ')' after column definitions", current_.position);
    }
    Advance();

    if (!IsExpectedToken(TokenType::Semicolon)) {
        throw ParseError("Expected ';' after CREATE TABLE statement", current_.position);
    }
    Advance();

    return statement;
}

std::unique_ptr<InsertStatement> Parser::ParseInsertStatement() {
    if (!IsExpectedToken(TokenType::Insert)) {
        throw ParseError("Expected INSERT", current_.position);
    }
    Advance();

    if (!IsExpectedToken(TokenType::Into)) {
        throw ParseError("Expected INTO after INSERT", current_.position);
    }
    Advance();

    if (!IsExpectedToken(TokenType::Identifier)) {
        throw ParseError("Expected table name after INTO", current_.position);
    }

    auto statement = std::make_unique<InsertStatement>();
    statement->table_name = current_.text;
    Advance();

    if (!IsExpectedToken(TokenType::LParen)) {
        throw ParseError("Expected '(' before column list", current_.position);
    }
    Advance();

    if (!IsColumnNameToken()) {
        throw ParseError("Expected column name", current_.position);
    }

    statement->column_names.push_back(current_.text);
    Advance();

    while (IsExpectedToken(TokenType::Comma)) {
        Advance();

        if (!IsColumnNameToken()) {
            throw ParseError("Expected column name after ','", current_.position);
        }

        statement->column_names.push_back(current_.text);
        Advance();
    }

    if (!IsExpectedToken(TokenType::RParen)) {
        throw ParseError("Expected ')' after column list", current_.position);
    }
    Advance();

    if (!IsExpectedToken(TokenType::Values)) {
        throw ParseError("Expected VALUES after column list", current_.position);
    }
    Advance();

    if (!IsExpectedToken(TokenType::LParen)) {
        throw ParseError("Expected '(' before values list", current_.position);
    }
    Advance();

    statement->values.push_back(ParseLiteralExpression());

    while (IsExpectedToken(TokenType::Comma)) {
        Advance();
        statement->values.push_back(ParseLiteralExpression());
    }

    if (!IsExpectedToken(TokenType::RParen)) {
        throw ParseError("Expected ')' after values list", current_.position);
    }
    Advance();

    if (!IsExpectedToken(TokenType::Semicolon)) {
        throw ParseError("Expected ';' after INSERT statement", current_.position);
    }
    Advance();

    return statement;
}

std::unique_ptr<SelectStatement> Parser::ParseSelectStatement() {
    if (!IsExpectedToken(TokenType::Select)) {
        throw ParseError("Expected SELECT", current_.position);
    }
    Advance();

    auto statement = std::make_unique<SelectStatement>();

    statement->select_items.push_back(ParseSelectItem());
    while (IsExpectedToken(TokenType::Comma)) {
        Advance();
        statement->select_items.push_back(ParseSelectItem());
    }

    if (!IsExpectedToken(TokenType::From)) {
        throw ParseError("Expected FROM after SELECT items", current_.position);
    }
    Advance();

    if (!IsExpectedToken(TokenType::Identifier)) {
        throw ParseError("Expected table name after FROM", current_.position);
    }
    statement->table_name = current_.text;
    Advance();

    if (IsExpectedToken(TokenType::Where)) {
        Advance();
        statement->where_expression = ParseOrExpression();
    }

    if (IsExpectedToken(TokenType::Group)) {
        Advance();

        if (!IsExpectedToken(TokenType::By)) {
            throw ParseError("Expected BY after GROUP", current_.position);
        }
        Advance();

        statement->group_by_items.push_back(ParseOrExpression());
        while (IsExpectedToken(TokenType::Comma)) {
            Advance();
            statement->group_by_items.push_back(ParseOrExpression());
        }
    }

    if (IsExpectedToken(TokenType::Order)) {
        Advance();

        if (!IsExpectedToken(TokenType::By)) {
            throw ParseError("Expected BY after ORDER", current_.position);
        }
        Advance();

        statement->order_by_items.push_back(ParseOrderByItem());
        while (IsExpectedToken(TokenType::Comma)) {
            Advance();
            statement->order_by_items.push_back(ParseOrderByItem());
        }
    }

    if (IsExpectedToken(TokenType::Limit)) {
        Advance();

        if (!IsExpectedToken(TokenType::IntLiteral)) {
            throw ParseError("Expected integer literal after LIMIT", current_.position);
        }

        statement->limit = static_cast<std::size_t>(std::stoull(current_.text));
        Advance();
    }

    if (!IsExpectedToken(TokenType::Semicolon)) {
        throw ParseError("Expected ';' after SELECT statement, got token: " + current_.text, current_.position);
    }
    Advance();

    return statement;
}

bool Parser::IsExpectedToken(TokenType type) const noexcept {
    return current_.type == type;
}

void Parser::Advance() {
    current_ = lexer_.NextToken();
}

ColumnDefinition Parser::ParseColumnDefinition() {
    if (!IsColumnNameToken()) {
        throw ParseError("Expected column name", current_.position);
    }

    ColumnDefinition column;
    column.name = current_.text;
    Advance();

    if (IsExpectedToken(TokenType::StringType)) {
        column.type = DataType::String;
        Advance();
    } else if (IsExpectedToken(TokenType::Int64Type)) {
        column.type = DataType::Int64;
        Advance();
    } else if (IsExpectedToken(TokenType::DoubleType)) {
        column.type = DataType::Double;
        Advance();
    } else {
        throw ParseError("Expected column type", current_.position);
    }

    if (IsExpectedToken(TokenType::Key)) {
        column.is_key = true;
        Advance();
        return column;
    }

    if (IsExpectedToken(TokenType::Null)) {
        column.nullable = true;
        Advance();
        return column;
    }

    if (IsExpectedToken(TokenType::Not)) {
        Advance();

        if (!IsExpectedToken(TokenType::Null)) {
            throw ParseError("Expected NULL after NOT", current_.position);
        }

        column.nullable = false;
        Advance();
        return column;
    }

    return column;
}

std::unique_ptr<SelectItem> Parser::ParseSelectItem() {
    if (IsExpectedToken(TokenType::Star)) {
        auto item = std::make_unique<SelectItemStar>();
        Advance();
        return item;
    }

    AggregateKind aggregate_kind;
    bool is_aggregate = true;

    if (IsExpectedToken(TokenType::Count)) {
        aggregate_kind = AggregateKind::Count;
    } else if (IsExpectedToken(TokenType::Sum)) {
        aggregate_kind = AggregateKind::Sum;
    } else if (IsExpectedToken(TokenType::Avg)) {
        aggregate_kind = AggregateKind::Avg;
    } else if (IsExpectedToken(TokenType::Min)) {
        aggregate_kind = AggregateKind::Min;
    } else if (IsExpectedToken(TokenType::Max)) {
        aggregate_kind = AggregateKind::Max;
    } else {
        is_aggregate = false;
    }

    if (is_aggregate) {
        Advance();

        if (!IsExpectedToken(TokenType::LParen)) {
            throw ParseError("Expected '(' after aggregate function", current_.position);
        }
        Advance();

        auto item = std::make_unique<SelectAggregateExpression>();
        item->kind = aggregate_kind;
        item->expression = ParseOrExpression();

        if (!IsExpectedToken(TokenType::RParen)) {
            throw ParseError("Expected ')' after aggregate argument", current_.position);
        }
        Advance();

        return item;
    }

    auto item = std::make_unique<SelectItemExpression>();
    item->expression = ParseOrExpression();
    return item;
}

OrderByItem Parser::ParseOrderByItem() {
    OrderByItem item;
    item.expression = ParseOrExpression();

    if (IsExpectedToken(TokenType::Asc)) {
        item.direction = OrderDirection::Asc;
        Advance();
        return item;
    }

    if (IsExpectedToken(TokenType::Desc)) {
        item.direction = OrderDirection::Desc;
        Advance();
        return item;
    }

    return item;
}

std::unique_ptr<Expression> Parser::ParseLiteralExpression() {
    if (IsExpectedToken(TokenType::StringLiteral)) {
        auto literal = std::make_unique<StringLiteral>();
        literal->value = current_.text;
        Advance();
        return literal;
    }

    if (IsExpectedToken(TokenType::IntLiteral)) {
        auto literal = std::make_unique<IntLiteral>();
        literal->value = std::stoll(current_.text);
        Advance();
        return literal;
    }

    if (IsExpectedToken(TokenType::DoubleLiteral)) {
        auto literal = std::make_unique<DoubleLiteral>();
        literal->value = std::stod(current_.text);
        Advance();
        return literal;
    }

    if (IsExpectedToken(TokenType::Null)) {
        auto literal = std::make_unique<NullLiteral>();
        Advance();
        return literal;
    }

    throw ParseError("Expected literal value", current_.position);
}

std::unique_ptr<Expression> Parser::ParseOrExpression() {
    auto left = ParseAndExpression();

    while (IsExpectedToken(TokenType::Or)) {
        Advance();

        auto right = ParseAndExpression();

        auto expression = std::make_unique<BinaryExpression>();
        expression->operation = BinaryOperation::Or;
        expression->left = std::move(left);
        expression->right = std::move(right);

        left = std::move(expression);
    }

    return left;
}

std::unique_ptr<Expression> Parser::ParseAndExpression() {
    auto left = ParseNotExpression();

    while (IsExpectedToken(TokenType::And)) {
        Advance();

        auto right = ParseNotExpression();

        auto expression = std::make_unique<BinaryExpression>();
        expression->operation = BinaryOperation::And;
        expression->left = std::move(left);
        expression->right = std::move(right);

        left = std::move(expression);
    }

    return left;
}

std::unique_ptr<Expression> Parser::ParseNotExpression() {
    if (IsExpectedToken(TokenType::Not)) {
        Advance();

        auto expression = std::make_unique<UnaryExpression>();
        expression->operation = UnaryOperation::Not;
        expression->expression = ParseNotExpression();

        return expression;
    }

    return ParsePredicateExpression();
}

std::unique_ptr<Expression> Parser::ParsePredicateExpression() {
    auto left = ParseAtomicExpression();

    if (IsExpectedToken(TokenType::Is)) {
        Advance();

        bool is_not = false;
        if (IsExpectedToken(TokenType::Not)) {
            is_not = true;
            Advance();
        }

        if (!IsExpectedToken(TokenType::Null)) {
            throw ParseError("Expected NULL after IS or IS NOT", current_.position);
        }
        Advance();

        auto expression = std::make_unique<IsNullExpression>();
        expression->expression = std::move(left);
        expression->is_not = is_not;

        return expression;
    }

    BinaryOperation operation;
    bool has_operation = true;

    if (IsExpectedToken(TokenType::Equal)) {
        operation = BinaryOperation::Equal;
    } else if (IsExpectedToken(TokenType::NotEqual)) {
        operation = BinaryOperation::NotEqual;
    } else if (IsExpectedToken(TokenType::Less)) {
        operation = BinaryOperation::Less;
    } else if (IsExpectedToken(TokenType::LessEqual)) {
        operation = BinaryOperation::LessEqual;
    } else if (IsExpectedToken(TokenType::Greater)) {
        operation = BinaryOperation::Greater;
    } else if (IsExpectedToken(TokenType::GreaterEqual)) {
        operation = BinaryOperation::GreaterEqual;
    } else {
        has_operation = false;
    }

    if (!has_operation) {
        return left;
    }

    Advance();

    auto right = ParseAtomicExpression();

    auto expression = std::make_unique<BinaryExpression>();
    expression->operation = operation;
    expression->left = std::move(left);
    expression->right = std::move(right);

    return expression;
}

std::unique_ptr<Expression> Parser::ParseAtomicExpression() {
    if (IsExpectedToken(TokenType::LParen)) {
        Advance();

        auto expression = ParseOrExpression();

        if (!IsExpectedToken(TokenType::RParen)) {
            throw ParseError("Expected ')' after expression", current_.position);
        }
        Advance();

        return expression;
    }

    if (IsColumnNameToken()) {
        auto expression = std::make_unique<ColumnExpression>();
        expression->column_name = current_.text;
        Advance();
        return expression;
    }

    if (IsExpectedToken(TokenType::StringLiteral) ||
        IsExpectedToken(TokenType::IntLiteral) ||
        IsExpectedToken(TokenType::DoubleLiteral) ||
        IsExpectedToken(TokenType::Null)) {
        return ParseLiteralExpression();
    }

    throw ParseError("Expected atomic expression", current_.position);
}

bool Parser::IsColumnNameToken() const noexcept {
    return current_.type == TokenType::Identifier ||
           current_.type == TokenType::Key;
}

}