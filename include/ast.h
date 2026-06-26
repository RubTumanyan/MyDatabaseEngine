#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>

namespace mydb {

// ─────────────────────────────────────────────────────────────────
//  Expressions  (right side of WHERE, values in INSERT, etc.)
// ─────────────────────────────────────────────────────────────────

// Forward declare so nodes can hold each other
struct BinaryExpr;
struct UnaryExpr;
struct LiteralExpr;
struct ColumnRefExpr;

// A value in the AST is one of these four types
using Expr = std::variant<
    std::shared_ptr<LiteralExpr>,
    std::shared_ptr<ColumnRefExpr>,
    std::shared_ptr<BinaryExpr>,
    std::shared_ptr<UnaryExpr>
>;

// 42, 3.14, 'hello', true, false
struct LiteralExpr {
    enum class Kind { Integer, Float, String, Bool };
    Kind        kind;
    std::string value;  // raw text
};

// A column name reference: "age", "users.id"
struct ColumnRefExpr {
    std::string table;   // optional: "users" in "users.id"
    std::string column;  // "id", "age", "name"
};

// age >= 30,  name != 'Bob',  x AND y
struct BinaryExpr {
    std::string op;   // "=", "!=", "<", ">", "<=", ">=", "AND", "OR"
    Expr        left;
    Expr        right;
};

// NOT x,  -x
struct UnaryExpr {
    std::string op;   // "NOT", "-"
    Expr        operand;
};

// ─────────────────────────────────────────────────────────────────
//  Column definition (used in CREATE TABLE)
// ─────────────────────────────────────────────────────────────────
struct ColumnDef {
    std::string name;
    std::string type;       // "INT", "TEXT", "FLOAT", "BOOL"
    bool        primaryKey = false;
    bool        notNull    = false;
};

// ─────────────────────────────────────────────────────────────────
//  ORDER BY clause
// ─────────────────────────────────────────────────────────────────
struct OrderByClause {
    std::string column;
    bool        ascending = true;
};

// ─────────────────────────────────────────────────────────────────
//  Statements  (top-level AST nodes)
// ─────────────────────────────────────────────────────────────────

// SELECT id, name FROM users WHERE age > 30 ORDER BY name LIMIT 10
struct SelectStatement {
    std::vector<std::string> columns;     // ["id","name"] or ["*"]
    std::string              table;
    std::optional<Expr>      where;
    std::vector<OrderByClause> orderBy;
    std::optional<int>       limit;
};

// INSERT INTO users (id, name) VALUES (1, 'Alice')
struct InsertStatement {
    std::string              table;
    std::vector<std::string> columns;
    std::vector<Expr>        values;
};

// UPDATE users SET name = 'Bob' WHERE id = 1
struct UpdateStatement {
    std::string table;
    std::vector<std::pair<std::string, Expr>> assignments;  // col -> expr
    std::optional<Expr> where;
};

// DELETE FROM users WHERE id = 1
struct DeleteStatement {
    std::string         table;
    std::optional<Expr> where;
};

// CREATE TABLE users (id INT PRIMARY KEY, name TEXT NOT NULL)
struct CreateTableStatement {
    std::string             table;
    std::vector<ColumnDef>  columns;
};

// DROP TABLE users
struct DropTableStatement {
    std::string table;
};

// Any statement is one of these
using Statement = std::variant<
    SelectStatement,
    InsertStatement,
    UpdateStatement,
    DeleteStatement,
    CreateTableStatement,
    DropTableStatement
>;

}