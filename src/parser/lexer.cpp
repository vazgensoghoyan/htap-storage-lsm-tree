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

}