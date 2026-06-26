#include "lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>

namespace mydb {

// ── Keyword table ─────────────────────────────────────────────────
static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"SELECT", TokenType::SELECT}, {"INSERT", TokenType::INSERT},
    {"UPDATE", TokenType::UPDATE}, {"DELETE", TokenType::DELETE},
    {"CREATE", TokenType::CREATE}, {"DROP",   TokenType::DROP},
    {"TABLE",  TokenType::TABLE},  {"FROM",   TokenType::FROM},
    {"WHERE",  TokenType::WHERE},  {"INTO",   TokenType::INTO},
    {"VALUES", TokenType::VALUES}, {"SET",    TokenType::SET},
    {"AND",    TokenType::AND},    {"OR",     TokenType::OR},
    {"NOT",    TokenType::NOT},    {"ORDER",  TokenType::ORDER},
    {"BY",     TokenType::BY},     {"ASC",    TokenType::ASC},
    {"DESC",   TokenType::DESC},   {"LIMIT",  TokenType::LIMIT},
    {"JOIN",   TokenType::JOIN},   {"ON",     TokenType::ON},
    {"INNER",  TokenType::INNER},  {"LEFT",   TokenType::LEFT},
    {"TRUE",   TokenType::BOOL_TRUE},
    {"FALSE",  TokenType::BOOL_FALSE},
};

// ──> Constructor
Lexer::Lexer(const std::string& source) : src_(source) {}

// ──> Navigation
bool Lexer::isAtEnd()  const { return pos_ >= src_.size(); }
char Lexer::peek()     const { return isAtEnd() ? '\0' : src_[pos_]; }
char Lexer::peekNext() const {
    return (pos_ + 1 >= src_.size()) ? '\0' : src_[pos_ + 1];
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { ++line_; col_ = 1; }
    else           { ++col_;             }
    return c;
}

Token Lexer::makeToken(TokenType t, const std::string& val) const {
    return Token(t, val, line_, col_);
}

// ──> Skipping
void Lexer::skipWhitespace() {
    while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek())))
        advance();
}

void Lexer::skipLineComment() {
    while (!isAtEnd() && peek() != '\n')
        advance();
}

// ──> String reader  'hello world'
Token Lexer::readString() {
    int startLine = line_, startCol = col_;
    advance(); // consume opening '

    std::string value;
    while (!isAtEnd() && peek() != '\'') {
        // support escape sequences: \' \\ \n \t
        if (peek() == '\\') {
            advance();
            char esc = advance();
            switch (esc) {
                case '\'': value += '\''; break;
                case '\\': value += '\\'; break;
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                default:   value += esc;  break;
            }
        } else {
            value += advance();
        }
    }

    if (isAtEnd())
        throw std::runtime_error(
            "Unterminated string literal at line " + std::to_string(startLine)
            + ", col " + std::to_string(startCol));

    advance(); // consume closing '
    return Token(TokenType::STRING, value, startLine, startCol);
}

// ──> Number reader  42 or 3.14
Token Lexer::readNumber() {
    int startLine = line_, startCol = col_;
    std::string value;

    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())))
        value += advance();

    // optional decimal part
    if (!isAtEnd() && peek() == '.'
        && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        value += advance(); // consume '.'
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())))
            value += advance();
    }

    return Token(TokenType::NUMBER, value, startLine, startCol);
}

// ──> Identifier / keyword reader
Token Lexer::readIdentifierOrKeyword() {
    int startLine = line_, startCol = col_;
    std::string value;

    while (!isAtEnd() &&
           (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
        value += advance();

    // uppercase copy for case-insensitive keyword lookup
    std::string upper = value;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c){ return std::toupper(c); });

    TokenType type = classifyWord(upper);
    return Token(type, value, startLine, startCol);
}

TokenType Lexer::classifyWord(const std::string& upper) const {
    auto it = KEYWORDS.find(upper);
    return (it != KEYWORDS.end()) ? it->second : TokenType::IDENTIFIER;
}

// ──> Main function tokenize loop
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespace();
        if (isAtEnd()) break;

        // skip line comments
        if (peek() == '-' && peekNext() == '-') {
            skipLineComment();
            continue;
        }

        int  l  = line_;
        int  c  = col_;
        char ch = peek();

        // ──> Punctution
        if (ch == ',') { advance(); tokens.emplace_back(TokenType::COMMA,     ",", l, c); continue; }
        if (ch == ';') { advance(); tokens.emplace_back(TokenType::SEMICOLON, ";", l, c); continue; }
        if (ch == '(') { advance(); tokens.emplace_back(TokenType::LPAREN,    "(", l, c); continue; }
        if (ch == ')') { advance(); tokens.emplace_back(TokenType::RPAREN,    ")", l, c); continue; }
        if (ch == '.') { advance(); tokens.emplace_back(TokenType::DOT,       ".", l, c); continue; }
        if (ch == '+') { advance(); tokens.emplace_back(TokenType::PLUS,      "+", l, c); continue; }
        if (ch == '/') { advance(); tokens.emplace_back(TokenType::SLASH,     "/", l, c); continue; }
        if (ch == '*') { advance(); tokens.emplace_back(TokenType::STAR,      "*", l, c); continue; }

        // ──> One or two char operators
        if (ch == '=') { advance(); tokens.emplace_back(TokenType::EQ,  "=",  l, c); continue; }
        if (ch == '-') { advance(); tokens.emplace_back(TokenType::MINUS,"-",  l, c); continue; }

        if (ch == '>') {
            advance();
            if (!isAtEnd() && peek() == '=') { advance(); tokens.emplace_back(TokenType::GTE, ">=", l, c); }
            else                              {            tokens.emplace_back(TokenType::GT,  ">",  l, c); }
            continue;
        }
        if (ch == '<') {
            advance();
            if (!isAtEnd() && peek() == '=') { advance(); tokens.emplace_back(TokenType::LTE, "<=", l, c); }
            else                              {            tokens.emplace_back(TokenType::LT,  "<",  l, c); }
            continue;
        }
        if (ch == '!') {
            advance();
            if (!isAtEnd() && peek() == '=') { advance(); tokens.emplace_back(TokenType::NEQ, "!=", l, c); }
            else throw std::runtime_error(
                "Expected '=' after '!' at line " + std::to_string(l));
            continue;
        }

        // ──> Literals
        if (ch == '\'') { tokens.push_back(readString()); continue; }

        if (std::isdigit(static_cast<unsigned char>(ch))) {
            tokens.push_back(readNumber());
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }

        // ── Unknown character ─────────────────
        throw std::runtime_error(
            std::string("Unknown character '") + ch +
            "' at line " + std::to_string(l) +
            ", col "     + std::to_string(c));
    }

    tokens.emplace_back(TokenType::END_OF_FILE, "", line_, col_);
    return tokens;
}

// ── Debug helper ──────────────────────────────────────────────────
std::string tokenTypeToString(TokenType t) {
    switch (t) {
        case TokenType::SELECT:     return "SELECT";
        case TokenType::INSERT:     return "INSERT";
        case TokenType::UPDATE:     return "UPDATE";
        case TokenType::DELETE:     return "DELETE";
        case TokenType::CREATE:     return "CREATE";
        case TokenType::DROP:       return "DROP";
        case TokenType::TABLE:      return "TABLE";
        case TokenType::FROM:       return "FROM";
        case TokenType::WHERE:      return "WHERE";
        case TokenType::INTO:       return "INTO";
        case TokenType::VALUES:     return "VALUES";
        case TokenType::SET:        return "SET";
        case TokenType::AND:        return "AND";
        case TokenType::OR:         return "OR";
        case TokenType::NOT:        return "NOT";
        case TokenType::ORDER:      return "ORDER";
        case TokenType::BY:         return "BY";
        case TokenType::LIMIT:      return "LIMIT";
        case TokenType::JOIN:       return "JOIN";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER:     return "NUMBER";
        case TokenType::STRING:     return "STRING";
        case TokenType::BOOL_TRUE:  return "TRUE";
        case TokenType::BOOL_FALSE: return "FALSE";
        case TokenType::EQ:         return "EQ(=)";
        case TokenType::NEQ:        return "NEQ(!=)";
        case TokenType::LT:         return "LT(<)";
        case TokenType::GT:         return "GT(>)";
        case TokenType::LTE:        return "LTE(<=)";
        case TokenType::GTE:        return "GTE(>=)";
        case TokenType::PLUS:       return "PLUS(+)";
        case TokenType::MINUS:      return "MINUS(-)";
        case TokenType::STAR:       return "STAR(*)";
        case TokenType::COMMA:      return "COMMA(,)";
        case TokenType::SEMICOLON:  return "SEMICOLON(;)";
        case TokenType::LPAREN:     return "LPAREN(()";
        case TokenType::RPAREN:     return "RPAREN())";
        case TokenType::DOT:        return "DOT(.)";
        case TokenType::END_OF_FILE:return "EOF";
        default:                    return "UNKNOWN";
    }
}

}