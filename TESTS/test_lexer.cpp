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

    // Распечатаем все токены чтобы посчитать правильно:
    // [0] SELECT
    // [1] *
    // [2] FROM
    // [3] users
    // [4] WHERE
    // [5] age
    // [6] >=
    // [7] 30
    // [8] AND
    // [9] name
    // [10] !=
    // [11] 'Bob'
    // [12] ;
    // [13] EOF

    EXPECT_EQ(tokens[0].type, TokenType::SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::STAR);
    EXPECT_EQ(tokens[2].type, TokenType::FROM);
    EXPECT_EQ(tokens[3].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[3].value, "users");
    EXPECT_EQ(tokens[4].type, TokenType::WHERE);
    EXPECT_EQ(tokens[5].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[5].value, "age");
    EXPECT_EQ(tokens[6].type, TokenType::GTE);
    EXPECT_EQ(tokens[7].value, "30");
    EXPECT_EQ(tokens[8].type, TokenType::AND);
    EXPECT_EQ(tokens[9].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[9].value, "name");
    EXPECT_EQ(tokens[10].type, TokenType::NEQ);
    EXPECT_EQ(tokens[11].value, "Bob");
    EXPECT_EQ(tokens[11].type, TokenType::STRING);
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
}// placeholder
