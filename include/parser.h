#pragma once
#include "token.h"
#include "ast.h"
#include <vector>
#include <stdexcept>

namespace mydb {

// ── Custom exception ──────────────────────────────────────────────
class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& msg)
        : std::runtime_error(msg) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // Main entry point — call once, returns one parsed statement
    Statement parse();

private:
    std::vector<Token> tokens_;
    size_t             pos_ = 0;

    // ── Navigation ────────────────────────────────────────────────
    const Token& peek()                const;
    const Token& peekNext()            const;
    const Token& advance();
    bool         isAtEnd()             const;
    bool         check(TokenType type) const;

    // consume if matches type, else throw
    const Token& expect(TokenType type, const std::string& msg);
    // consume if matches type, return true/false
    bool         match(TokenType type);

    // ── Statement parsers ─────────────────────────────────────────
    Statement            parseStatement();
    SelectStatement      parseSelect();
    InsertStatement      parseInsert();
    UpdateStatement      parseUpdate();
    DeleteStatement      parseDelete();
    CreateTableStatement parseCreateTable();
    DropTableStatement   parseDropTable();

    // ── Clause parsers ────────────────────────────────────────────
    std::vector<std::string>   parseColumnList();
    std::optional<Expr>        parseWhereClause();
    std::vector<OrderByClause> parseOrderByClause();
    std::optional<int>         parseLimitClause();
    ColumnDef                  parseColumnDef();

    // ── Expression parsers (precedence ladder) ────────────────────
    Expr parseExpression();  // AND / OR
    Expr parseComparison();  // = != < > <= >=
    Expr parseUnary();       // NOT  -
    Expr parsePrimary();     // literal, column, (expr)

    // ── Error helper ──────────────────────────────────────────────
    ParseError error(const std::string& msg) const;
};

}