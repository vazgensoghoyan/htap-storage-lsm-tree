#pragma once

#include <string>

#include "parser/error.hpp"
#include "parser/token.hpp"

namespace htap::parser {

class Lexer {
public:
    explicit Lexer(std::string input);

    Token NextToken();

private:
    bool IsAtEnd() const noexcept;
    char CurrentChar() const noexcept;
    char PeekChar() const noexcept;

    void Advance() noexcept;
    void SkipWhitespace() noexcept;

    Token ReadIdentifierOrKeyword();
    Token ReadNumber();
    Token ReadString();

    Token MakeToken(TokenType type, SourcePosition start, std::string text) const;
    SourcePosition CurrentPosition() const noexcept;

    static bool IsIdentifierStart(char c) noexcept;
    static bool IsIdentifierPart(char c) noexcept;
    static TokenType KeywordType(const std::string& text) noexcept;

private:
    std::string input_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t position_in_line_ = 1;

    static std::string ToUpper(const std::string& text);
};

}