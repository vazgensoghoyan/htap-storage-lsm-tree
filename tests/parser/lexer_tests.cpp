#include <string>

#include <gtest/gtest.h>

#include "parser/error.hpp"
#include "parser/lexer.hpp"
#include "parser/token.hpp"

namespace htap::parser {
namespace {

void ExpectToken(Lexer& lexer, TokenType expected_type, const std::string& expected_text) {
    const Token token = lexer.NextToken();

    EXPECT_EQ(token.type, expected_type);
    EXPECT_EQ(token.text, expected_text);
}

TEST(LexerTest, TokenizesSimpleSelectQuery) {
    Lexer lexer("SELECT name FROM users;");

    ExpectToken(lexer, TokenType::Select, "SELECT");
    ExpectToken(lexer, TokenType::Identifier, "name");
    ExpectToken(lexer, TokenType::From, "FROM");
    ExpectToken(lexer, TokenType::Identifier, "users");
    ExpectToken(lexer, TokenType::Semicolon, ";");
    ExpectToken(lexer, TokenType::EndOfFile, "");
}

TEST(LexerTest, TokenizesNegativeDoubleLiteral) {
    Lexer lexer("-12.5");

    ExpectToken(lexer, TokenType::DoubleLiteral, "-12.5");
    ExpectToken(lexer, TokenType::EndOfFile, "");
}

TEST(LexerTest, TokenizesPostgresStyleStringLiteral) {
    Lexer lexer("'I''m fine'");

    ExpectToken(lexer, TokenType::StringLiteral, "I'm fine");
    ExpectToken(lexer, TokenType::EndOfFile, "");
}

TEST(LexerTest, TokenizesComparisonOperators) {
    Lexer lexer("<= >= != = < >");

    ExpectToken(lexer, TokenType::LessEqual, "<=");
    ExpectToken(lexer, TokenType::GreaterEqual, ">=");
    ExpectToken(lexer, TokenType::NotEqual, "!=");
    ExpectToken(lexer, TokenType::Equal, "=");
    ExpectToken(lexer, TokenType::Less, "<");
    ExpectToken(lexer, TokenType::Greater, ">");
    ExpectToken(lexer, TokenType::EndOfFile, "");
}

TEST(LexerTest, ThrowsOnUnexpectedCharacter) {
    Lexer lexer("@");

    EXPECT_THROW(
        {
            lexer.NextToken();
        },
        ParseError
    );
}

}
}
