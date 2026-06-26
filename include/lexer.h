#pragma once
#include "token.h"
#include <string>
#include <vector>

namespace mydb {

    class Lexer {
    public:
        explicit Lexer(const std::string& source);

        // return all tokens
        std::vector<Token> tokenize();

    private:
        std::string src_;  // SQL query
        size_t      pos_  = 0; // current possition
        int         line_ = 1; //
        int         col_  = 1;

        // ──> Navigation
        bool isAtEnd()    const;
        char peek()       const;   // current char, don't consume
        char peekNext()   const;   // one ahead, don't consume
        char advance();            // consume and return current char

        // ──-> Skipping
        void skipWhitespace(); // "\n" "\t"
        void skipLineComment();    // anything after --

        // ──> Readers
        Token readString();               // 'hello world'
        Token readNumber();               // 42 or 3.14
        Token readIdentifierOrKeyword();  // SELECT, FROM, users ...

        // ──> Helpers
        TokenType classifyWord(const std::string& upper) const;
        Token     makeToken(TokenType t, const std::string& val) const;
    };

}