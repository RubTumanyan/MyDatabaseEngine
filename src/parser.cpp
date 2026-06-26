#include "parser.h"
#include <stdexcept>

namespace mydb {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

// ── Navigation ────────────────────────────────────────────────────
bool Parser::isAtEnd() const {
    return tokens_[pos_].type == TokenType::END_OF_FILE;
}

const Token& Parser::peek() const {
    return tokens_[pos_];
}

const Token& Parser::peekNext() const {
    if (pos_ + 1 >= tokens_.size()) return tokens_.back();
    return tokens_[pos_ + 1];
}

const Token& Parser::advance() {
    if (!isAtEnd()) ++pos_;
    return tokens_[pos_ - 1];  // возвращаем токен КОТОРЫЙ только что потребили
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& msg) {
    if (check(type)) return advance();
    throw error(msg + " — got '" + peek().value + "'");
}

ParseError Parser::error(const std::string& msg) const {
    return ParseError(
        "Parse error at line " + std::to_string(peek().line) +
        ", col "               + std::to_string(peek().col)  +
        ": "                   + msg
    );
}

// ── Entry point ───────────────────────────────────────────────────
Statement Parser::parse() {
    Statement stmt = parseStatement();
    match(TokenType::SEMICOLON); // опциональная точка с запятой в конце
    return stmt;
}

// ── Statement dispatcher ──────────────────────────────────────────
// смотрим на первый токен и решаем какой оператор парсить
Statement Parser::parseStatement() {
    switch (peek().type) {
        case TokenType::SELECT: return parseSelect();
        case TokenType::INSERT: return parseInsert();
        case TokenType::UPDATE: return parseUpdate();
        case TokenType::DELETE: return parseDelete();
        case TokenType::CREATE: return parseCreateTable();
        case TokenType::DROP:   return parseDropTable();
        default:
            throw error("Expected SELECT, INSERT, UPDATE, DELETE, CREATE or DROP");
    }
}

// ── SELECT ────────────────────────────────────────────────────────
// SELECT col1, col2 FROM table [WHERE expr]
//        [ORDER BY col [ASC|DESC]] [LIMIT n]
SelectStatement Parser::parseSelect() {
    expect(TokenType::SELECT, "Expected SELECT");
    SelectStatement stmt;

    // колонки: либо * либо список имён
    if (check(TokenType::STAR)) {
        advance();
        stmt.columns.push_back("*");
    } else {
        stmt.columns = parseColumnList();
    }

    expect(TokenType::FROM, "Expected FROM after column list");
    stmt.table = expect(TokenType::IDENTIFIER, "Expected table name").value;

    // все остальные части опциональны
    stmt.where   = parseWhereClause();
    stmt.orderBy = parseOrderByClause();
    stmt.limit   = parseLimitClause();

    return stmt;
}

// ── INSERT ────────────────────────────────────────────────────────
// INSERT INTO table (col1, col2) VALUES (val1, val2)
InsertStatement Parser::parseInsert() {
    expect(TokenType::INSERT, "Expected INSERT");
    expect(TokenType::INTO,   "Expected INTO after INSERT");

    InsertStatement stmt;
    stmt.table = expect(TokenType::IDENTIFIER, "Expected table name").value;

    // список колонок в скобках
    expect(TokenType::LPAREN, "Expected '(' before column list");
    stmt.columns = parseColumnList();
    expect(TokenType::RPAREN, "Expected ')' after column list");

    expect(TokenType::VALUES, "Expected VALUES");

    // список значений в скобках
    expect(TokenType::LPAREN, "Expected '(' before values");
    do {
        stmt.values.push_back(parseExpression());
    } while (match(TokenType::COMMA));
    expect(TokenType::RPAREN, "Expected ')' after values");

    return stmt;
}

// ── UPDATE ────────────────────────────────────────────────────────
// UPDATE table SET col1 = val1, col2 = val2 [WHERE expr]
UpdateStatement Parser::parseUpdate() {
    expect(TokenType::UPDATE, "Expected UPDATE");

    UpdateStatement stmt;
    stmt.table = expect(TokenType::IDENTIFIER, "Expected table name").value;
    expect(TokenType::SET, "Expected SET after table name");

    // одно или несколько присваиваний col = val
    do {
        std::string col = expect(TokenType::IDENTIFIER, "Expected column name").value;
        expect(TokenType::EQ, "Expected '=' after column name");
        Expr val = parseExpression();
        stmt.assignments.emplace_back(col, std::move(val));
    } while (match(TokenType::COMMA));

    stmt.where = parseWhereClause();
    return stmt;
}

// ── DELETE ────────────────────────────────────────────────────────
// DELETE FROM table [WHERE expr]
DeleteStatement Parser::parseDelete() {
    expect(TokenType::DELETE, "Expected DELETE");
    expect(TokenType::FROM,   "Expected FROM after DELETE");

    DeleteStatement stmt;
    stmt.table = expect(TokenType::IDENTIFIER, "Expected table name").value;
    stmt.where = parseWhereClause();
    return stmt;
}

// ── CREATE TABLE ──────────────────────────────────────────────────
// CREATE TABLE users (id INT PRIMARY KEY, name TEXT NOT NULL)
CreateTableStatement Parser::parseCreateTable() {
    expect(TokenType::CREATE, "Expected CREATE");
    expect(TokenType::TABLE,  "Expected TABLE after CREATE");

    CreateTableStatement stmt;
    stmt.table = expect(TokenType::IDENTIFIER, "Expected table name").value;

    expect(TokenType::LPAREN, "Expected '(' before column definitions");
    do {
        stmt.columns.push_back(parseColumnDef());
    } while (match(TokenType::COMMA));
    expect(TokenType::RPAREN, "Expected ')' after column definitions");

    return stmt;
}

// парсим одно определение колонки:  id INT PRIMARY KEY
ColumnDef Parser::parseColumnDef() {
    ColumnDef def;
    def.name = expect(TokenType::IDENTIFIER, "Expected column name").value;
    def.type = expect(TokenType::IDENTIFIER, "Expected type (INT, TEXT...)").value;

    // опциональные ограничения
    while (true) {
        if (peek().value == "PRIMARY") {
            advance(); // PRIMARY
            expect(TokenType::IDENTIFIER, "Expected KEY after PRIMARY");
            def.primaryKey = true;
        } else if (check(TokenType::NOT)) {
            advance(); // NOT
            expect(TokenType::IDENTIFIER, "Expected NULL after NOT");
            def.notNull = true;
        } else break;
    }
    return def;
}

// ── DROP TABLE ────────────────────────────────────────────────────
DropTableStatement Parser::parseDropTable() {
    expect(TokenType::DROP,  "Expected DROP");
    expect(TokenType::TABLE, "Expected TABLE after DROP");

    DropTableStatement stmt;
    stmt.table = expect(TokenType::IDENTIFIER, "Expected table name").value;
    return stmt;
}

// ── Clause helpers ────────────────────────────────────────────────
std::vector<std::string> Parser::parseColumnList() {
    std::vector<std::string> cols;
    do {
        cols.push_back(
            expect(TokenType::IDENTIFIER, "Expected column name").value
        );
    } while (match(TokenType::COMMA));
    return cols;
}

std::optional<Expr> Parser::parseWhereClause() {
    if (!match(TokenType::WHERE)) return std::nullopt;
    return parseExpression();
}

std::vector<OrderByClause> Parser::parseOrderByClause() {
    std::vector<OrderByClause> clauses;
    if (!match(TokenType::ORDER)) return clauses;
    expect(TokenType::BY, "Expected BY after ORDER");
    do {
        OrderByClause c;
        c.column    = expect(TokenType::IDENTIFIER, "Expected column name").value;
        c.ascending = !match(TokenType::DESC); // по умолчанию ASC
        match(TokenType::ASC);                 // потребляем ASC если есть
        clauses.push_back(c);
    } while (match(TokenType::COMMA));
    return clauses;
}

std::optional<int> Parser::parseLimitClause() {
    if (!match(TokenType::LIMIT)) return std::nullopt;
    std::string num = expect(TokenType::NUMBER, "Expected number after LIMIT").value;
    return std::stoi(num);
}

// ── Expression parsing — precedence ladder ────────────────────────
//
//  Почему лестница?
//  age >= 30 AND name != 'Bob'
//  Сначала должны парситься >= и != (высокий приоритет)
//  Потом из результатов собирается AND (низкий приоритет)
//
//  parseExpression  →  AND / OR        (низший приоритет)
//  parseComparison  →  = != < > <= >=
//  parseUnary       →  NOT  -
//  parsePrimary     →  42, 'Bob', age  (высший приоритет)

Expr Parser::parseExpression() {
    Expr left = parseComparison(); // сначала парсим левую часть

    // потом смотрим есть ли AND/OR
    while (check(TokenType::AND) || check(TokenType::OR)) {
        std::string op = advance().value; // "AND" или "OR"
        Expr right = parseComparison();   // парсим правую часть

        // собираем узел дерева
        auto node   = std::make_shared<BinaryExpr>();
        node->op    = op;
        node->left  = std::move(left);
        node->right = std::move(right);
        left = node; // результат становится новой левой частью
    }
    return left;
}

Expr Parser::parseComparison() {
    Expr left = parseUnary();

    while (check(TokenType::EQ)  || check(TokenType::NEQ) ||
           check(TokenType::LT)  || check(TokenType::GT)  ||
           check(TokenType::LTE) || check(TokenType::GTE)) {
        std::string op = advance().value;
        Expr right = parseUnary();

        auto node   = std::make_shared<BinaryExpr>();
        node->op    = op;
        node->left  = std::move(left);
        node->right = std::move(right);
        left = node;
    }
    return left;
}

Expr Parser::parseUnary() {
    if (check(TokenType::NOT) || check(TokenType::MINUS)) {
        std::string op = advance().value;
        Expr operand   = parseUnary(); // рекурсия: NOT NOT x

        auto node     = std::make_shared<UnaryExpr>();
        node->op      = op;
        node->operand = std::move(operand);
        return node;
    }
    return parsePrimary();
}

Expr Parser::parsePrimary() {
    // Число: 42 или 3.14
    if (check(TokenType::NUMBER)) {
        auto tok = advance();
        auto lit = std::make_shared<LiteralExpr>();
        lit->value = tok.value;
        lit->kind  = (tok.value.find('.') != std::string::npos)
                     ? LiteralExpr::Kind::Float
                     : LiteralExpr::Kind::Integer;
        return lit;
    }

    // Строка: 'hello'
    if (check(TokenType::STRING)) {
        auto tok = advance();
        auto lit = std::make_shared<LiteralExpr>();
        lit->value = tok.value;
        lit->kind  = LiteralExpr::Kind::String;
        return lit;
    }

    // Булево значение: true / false
    if (check(TokenType::BOOL_TRUE) || check(TokenType::BOOL_FALSE)) {
        auto tok = advance();
        auto lit = std::make_shared<LiteralExpr>();
        lit->value = tok.value;
        lit->kind  = LiteralExpr::Kind::Bool;
        return lit;
    }

    // Ссылка на колонку: age  или  users.id
    if (check(TokenType::IDENTIFIER)) {
        auto col    = std::make_shared<ColumnRefExpr>();
        col->column = advance().value;

        // опциональный префикс таблицы: users.id
        if (check(TokenType::DOT)) {
            advance(); // потребляем '.'
            col->table  = col->column;
            col->column = expect(TokenType::IDENTIFIER,
                                 "Expected column name after '.'").value;
        }
        return col;
    }

    // Выражение в скобках: ( expr )
    if (check(TokenType::LPAREN)) {
        advance(); // потребляем '('
        Expr inner = parseExpression();
        expect(TokenType::RPAREN, "Expected ')' after expression");
        return inner;
    }

    throw error("Expected a value: number, string, column name, or '('");
}

} // namespace mydb