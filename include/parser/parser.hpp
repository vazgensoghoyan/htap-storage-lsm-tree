#pragma once

#include <memory>

#include "parser/ast.hpp"
#include "parser/error.hpp"
#include "parser/lexer.hpp"
#include "parser/token.hpp"

namespace htap::parser {

class Parser {
public:
    explicit Parser(Lexer lexer);

    std::unique_ptr<Statement> ParseStatement();

private:
    std::unique_ptr<CreateTableStatement> ParseCreateTableStatement();
    std::unique_ptr<InsertStatement> ParseInsertStatement();
    std::unique_ptr<SelectStatement> ParseSelectStatement();

    bool IsExpectedToken(TokenType type) const noexcept;
    void Advance();
    bool IsColumnNameToken() const noexcept;

    ColumnDefinition ParseColumnDefinition();
    std::unique_ptr<SelectItem> ParseSelectItem();
    OrderByItem ParseOrderByItem();

    std::unique_ptr<Expression> ParseLiteralExpression();

    std::unique_ptr<Expression> ParseOrExpression();
    std::unique_ptr<Expression> ParseAndExpression();
    std::unique_ptr<Expression> ParseNotExpression();
    std::unique_ptr<Expression> ParsePredicateExpression();
    std::unique_ptr<Expression> ParseAtomicExpression();

private:
    Lexer lexer_;
    Token current_;
};

} 
