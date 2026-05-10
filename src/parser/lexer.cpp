#include "parser/lexer.hpp"
#include <utility>
#include <cctype>

namespace htap::parser {

Lexer::Lexer(std::string input)
    : input_(std::move(input)) {
}

bool Lexer::IsAtEnd() const noexcept {
    return pos_ >= input_.size();
}

char Lexer::CurrentChar() const noexcept {
    if (IsAtEnd()) return '\0';

    return input_[pos_];
}

char Lexer::PeekChar() const noexcept {
    if (pos_ + 1 >= input_.size()) return '\0';

    return input_[pos_ + 1];
}

void Lexer::Advance() noexcept {
    if (IsAtEnd()) return;

    if (CurrentChar() == '\n') {
        ++line_;
        position_in_line_ = 1;
    } else {
        ++position_in_line_;
    }

    ++pos_;
}

void Lexer::SkipWhitespace() noexcept {
    while (!IsAtEnd() && (CurrentChar() == ' ' || CurrentChar() == '\t'
        || CurrentChar() == '\n' || CurrentChar() == '\r')) {
            Advance();
        }
}

SourcePosition Lexer::CurrentPosition() const noexcept {
    SourcePosition position;
    position.pos = pos_;
    position.line = line_;
    position.position_in_line = position_in_line_;
    return position;
}

Token Lexer::MakeToken(TokenType type, SourcePosition start, std::string text) const {
    Token token;
    token.position = start;
    token.type = type;
    token.text = std::move(text);

    return token;
}

std::string Lexer::ToUpper(const std::string& text) {
    std::string result;
    result.reserve(text.size());

    for (char c : text) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    return result;

}

TokenType Lexer::KeywordType(const std::string& text) noexcept {
    const std::string upper = ToUpper(text);

    if (upper == "CREATE") {
        return TokenType::Create;
    }
    if (upper == "TABLE") {
        return TokenType::Table;
    }
    if (upper == "INSERT") {
        return TokenType::Insert;
    }
    if (upper == "INTO") {
        return TokenType::Into;
    }
    if (upper == "VALUES") {
        return TokenType::Values;
    }
    if (upper == "SELECT") {
        return TokenType::Select;
    }
    if (upper == "FROM") {
        return TokenType::From;
    }
    if (upper == "WHERE") {
        return TokenType::Where;
    }
    if (upper == "KEY") {
        return TokenType::Key;
    }
    if (upper == "PRIMARY") {
        return TokenType::Primary;
    }
    if (upper == "NULL") {
        return TokenType::Null;
    }
    if (upper == "NOT") {
        return TokenType::Not;
    }
    if (upper == "AND") {
        return TokenType::And;
    }
    if (upper == "OR") {
        return TokenType::Or;
    }
    if (upper == "IS") {
        return TokenType::Is;
    }
    if (upper == "STRING") {
        return TokenType::StringType;
    }
    if (upper == "INT64") {
        return TokenType::Int64Type;
    }
    if (upper == "DOUBLE") {
        return TokenType::DoubleType;
    }
    if (upper == "COUNT") {
        return TokenType::Count;
    }
    if (upper == "SUM") {
        return TokenType::Sum;
    }
    if (upper == "AVG") {
        return TokenType::Avg;
    }
    if (upper == "MIN") {
        return TokenType::Min;
    }
    if (upper == "MAX") {
        return TokenType::Max;
    }
    if (upper == "GROUP") {
        return TokenType::Group;
    }
    if (upper == "BY") {
        return TokenType::By;
    }
    if (upper == "ORDER") {
        return TokenType::Order;
    }
    if (upper == "LIMIT") {
        return TokenType::Limit;
    }
    if (upper == "ASC") {
        return TokenType::Asc;
    }
    if (upper == "DESC") {
        return TokenType::Desc;
    }

    return TokenType::Identifier;
}

bool Lexer::IsIdentifierStart(char c) noexcept {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool Lexer::IsIdentifierPart(char c) noexcept {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

Token Lexer::ReadIdentifierOrKeyword() {
    const SourcePosition start = CurrentPosition();
    const std::size_t start_pos = pos_;

    while (!IsAtEnd() && IsIdentifierPart(CurrentChar())) {
        Advance();
    }

    const std::string text = input_.substr(start_pos, pos_ - start_pos);
    const TokenType type = KeywordType(text);

    return MakeToken(type, start, text);
}

Token Lexer::ReadNumber()
{
    const SourcePosition token_start = CurrentPosition();
    bool is_double = false;

    if (CurrentChar() == '-') {
        Advance();
    }

    if (std::isdigit(static_cast<unsigned char>(CurrentChar())) == 0) {
        throw ParseError("ReadNumber called at invalid position", token_start);
    }

    while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(CurrentChar())) != 0) {
        Advance();
    }

    if (!IsAtEnd() && CurrentChar() == '.')
    {
        is_double = true;
        Advance();

        if (std::isdigit(static_cast<unsigned char>(CurrentChar())) == 0) {
            throw ParseError("ReadNumber called at invalid position", token_start);
        }

        while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(CurrentChar())) != 0) {
            Advance();
        }
    }

    const std::string text = input_.substr(token_start.pos, pos_ - token_start.pos);

    if (is_double) {
        return MakeToken(TokenType::DoubleLiteral, token_start, text);
    }

    return MakeToken(TokenType::IntLiteral, token_start, text);
}

Token Lexer::ReadString() {
    const SourcePosition token_start = CurrentPosition();

    if (CurrentChar() != '\'') {
        throw ParseError("ReadString called at invalid position", token_start);
    }

    Advance();

    std::string text;

    while (!IsAtEnd()) {
        if (CurrentChar() == '\'') {
            if (PeekChar() == '\'') {
                text.push_back('\'');
                Advance();
                Advance();
                continue;
            }

            Advance();
            return MakeToken(TokenType::StringLiteral, token_start, text);
        }

        text.push_back(CurrentChar());
        Advance();
    }

    throw ParseError("Unterminated string literal", token_start);
}

Token Lexer::NextToken() {
    SkipWhitespace();

    const SourcePosition token_start = CurrentPosition();

    if (IsAtEnd()) {
        return MakeToken(TokenType::EndOfFile, token_start, "");
    }

    const char current = CurrentChar();

    if (IsIdentifierStart(current)) {
        return ReadIdentifierOrKeyword();
    }

    if (std::isdigit(static_cast<unsigned char>(current)) != 0 || 
        (current == '-' && std::isdigit(static_cast<unsigned char>(PeekChar())) != 0)) {
        return ReadNumber();
    }

    if (current == '\'') {
        return ReadString();
    }


    switch (current) {
        case '(':
            Advance();
            return MakeToken(TokenType::LParen, token_start, "(");

        case ')':
            Advance();
            return MakeToken(TokenType::RParen, token_start, ")");

        case ',':
            Advance();
            return MakeToken(TokenType::Comma, token_start, ",");

        case ';':
            Advance();
            return MakeToken(TokenType::Semicolon, token_start, ";");

        case '*':
            Advance();
            return MakeToken(TokenType::Star, token_start, "*");

        case '=':
            Advance();
            return MakeToken(TokenType::Equal, token_start, "=");

        case '!':
            Advance();
            if (CurrentChar() == '=') {
                Advance();
                return MakeToken(TokenType::NotEqual, token_start, "!=");
            }
            throw ParseError("Unexpected character '!'", token_start);

        case '<':
            Advance();
            if (CurrentChar() == '=') {
                Advance();
                return MakeToken(TokenType::LessEqual, token_start, "<=");
            }
            return MakeToken(TokenType::Less, token_start, "<");

        case '>':
            Advance();
            if (CurrentChar() == '=') {
                Advance();
                return MakeToken(TokenType::GreaterEqual, token_start, ">=");
            }
            return MakeToken(TokenType::Greater, token_start, ">");

        default:
            throw ParseError(
                std::string("Unexpected character: ") + current,
                token_start
            );
    }

}

}