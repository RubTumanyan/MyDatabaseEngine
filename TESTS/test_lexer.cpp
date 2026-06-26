#include <gtest/gtest.h>
#include "lexer.h"

using namespace mydb;

TEST(LexerTest, SimpleSelect) {
    Lexer lexer("SELECT id, name FROM users;");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].value, "id");
    EXPECT_EQ(tokens[2].type, TokenType::COMMA);
    EXPECT_EQ(tokens[3].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[3].value, "name");
    EXPECT_EQ(tokens[4].type, TokenType::FROM);
    EXPECT_EQ(tokens[5].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[5].value, "users");
}

TEST(LexerTest, WhereClauseWithOperators) {
    Lexer lexer("SELECT * FROM users WHERE age >= 30 AND name != 'Bob';");
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::STAR);
    EXPECT_EQ(tokens[6].type, TokenType::WHERE);
    EXPECT_EQ(tokens[8].type, TokenType::GTE);
    EXPECT_EQ(tokens[9].value, "30");
    EXPECT_EQ(tokens[10].type, TokenType::AND);
    EXPECT_EQ(tokens[12].type, TokenType::NEQ);
    EXPECT_EQ(tokens[13].value, "Bob");
    EXPECT_EQ(tokens[13].type, TokenType::STRING);
}

TEST(LexerTest, CaseInsensitiveKeywords) {
    Lexer lexer("select * from users");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::SELECT);
    EXPECT_EQ(tokens[2].type, TokenType::FROM);
}

TEST(LexerTest, LineComment) {
    Lexer lexer("SELECT id -- this is a comment\nFROM users;");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[2].type, TokenType::FROM); // comment skipped
}

TEST(LexerTest, UnterminatedStringThrows) {
    Lexer lexer("SELECT 'unterminated");
    EXPECT_THROW(lexer.tokenize(), std::runtime_error);
}