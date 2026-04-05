#pragma once

#include <cstddef>
#include <string>

namespace htap::parser {

struct SourcePosition {
    std::size_t pos = 0;
    std::size_t line = 1;
    std::size_t position_in_line = 1;
};

enum class TokenType {
    EndOfFile,
    Invalid,

    Identifier,
    StringLiteral,
    IntLiteral,
    DoubleLiteral,

    Create,
    Table,
    Insert,
    Into,
    Values,
    Select,
    From,
    Where,
    Key,
    Primary,
    Null,
    Not,
    And,
    Or,
    Is,
    Group,
    By,
    Order,
    Limit,

    Asc,
    Desc,

    StringType,
    Int64Type,
    DoubleType,

    Count,
    Sum,
    Avg,
    Min,
    Max,

    LParen,
    RParen,
    Comma,
    Semicolon,
    Star,

    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
};

struct Token {
    TokenType type = TokenType::Invalid;
    SourcePosition position;
    std::string text;
};

}