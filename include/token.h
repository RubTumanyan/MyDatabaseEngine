#pragma once
#pragma once
#include <string>

namespace mydb {

    enum class TokenType {
        // ──> Keywords
        SELECT, INSERT, UPDATE, DELETE,
        CREATE, DROP, TABLE,
        FROM, WHERE, INTO, VALUES, SET,
        AND, OR, NOT,
        ORDER, BY, ASC, DESC,
        LIMIT, JOIN, ON, INNER, LEFT,

        // ──> Literals
        IDENTIFIER,   // users, age, id
        NUMBER,       // 42  or  3.14
        STRING,       // 'hello'
        BOOL_TRUE,    // true
        BOOL_FALSE,   // false

        // ──-> Operators
        EQ,           // =
        NEQ,          // !=
        LT,           // <
        GT,           // >
        LTE,          // <=
        GTE,          // >=
        PLUS,         // +
        MINUS,        // -
        SLASH,        // /
        STAR,         // *  (also multiply)

        // ─---> Punctuation
        COMMA,        // ,
        SEMICOLON,    // ;
        LPAREN,       // (
        RPAREN,       // )
        DOT,          // .

        // ──-> Special
        END_OF_FILE
    };

    // ──> Token
    struct Token {
        TokenType   type;
        std::string value;  // original text
        int         line;
        int         col;

        Token(TokenType t, std::string v, int l, int c)
            : type(t), value(std::move(v)), line(l), col(c) {}
    };

    // For printing / debugging
    std::string tokenTypeToString(TokenType t);

}