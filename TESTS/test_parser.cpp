#include <gtest/gtest.h>
#include "lexer.h"
#include "parser.h"

using namespace mydb;

// ── Helper: лексер + парсерgi за один вызов ────────────────────────
static Statement parseSQL(const std::string& sql) {
    Lexer  lexer(sql);
    Parser parser(lexer.tokenize());
    return parser.parse();
}

// ─────────────────────────────────────────────────────────────────
//  SELECT tests
// ─────────────────────────────────────────────────────────────────

TEST(ParserTest, SimpleSelect) {
    auto stmt = parseSQL("SELECT id, name FROM users;");
    auto& sel = std::get<SelectStatement>(stmt);

    EXPECT_EQ(sel.table,      "users");
    EXPECT_EQ(sel.columns.size(), 2);
    EXPECT_EQ(sel.columns[0], "id");
    EXPECT_EQ(sel.columns[1], "name");
    EXPECT_FALSE(sel.where.has_value());
    EXPECT_FALSE(sel.limit.has_value());
}

TEST(ParserTest, SelectStar) {
    auto stmt = parseSQL("SELECT * FROM products;");
    auto& sel = std::get<SelectStatement>(stmt);

    EXPECT_EQ(sel.table,      "products");
    EXPECT_EQ(sel.columns.size(), 1);
    EXPECT_EQ(sel.columns[0], "*");
}

TEST(ParserTest, SelectWithWhere) {
    auto stmt = parseSQL("SELECT * FROM users WHERE age >= 30;");
    auto& sel = std::get<SelectStatement>(stmt);

    EXPECT_TRUE(sel.where.has_value());

    // WHERE должен быть BinaryExpr с оператором >=
    auto& bin = *std::get<std::shared_ptr<BinaryExpr>>(sel.where.value());
    EXPECT_EQ(bin.op, ">=");

    // левая часть — колонка age
    auto& col = *std::get<std::shared_ptr<ColumnRefExpr>>(bin.left);
    EXPECT_EQ(col.column, "age");

    // правая часть — литерал 30
    auto& lit = *std::get<std::shared_ptr<LiteralExpr>>(bin.right);
    EXPECT_EQ(lit.value, "30");
    EXPECT_EQ(lit.kind,  LiteralExpr::Kind::Integer);
}

TEST(ParserTest, SelectWithAndCondition) {
    auto stmt = parseSQL(
        "SELECT * FROM users WHERE age >= 30 AND name != 'Bob';");
    auto& sel = std::get<SelectStatement>(stmt);

    EXPECT_TRUE(sel.where.has_value());

    // верхний узел — AND
    auto& top = *std::get<std::shared_ptr<BinaryExpr>>(sel.where.value());
    EXPECT_EQ(top.op, "AND");

    // левая ветка — age >= 30
    auto& left = *std::get<std::shared_ptr<BinaryExpr>>(top.left);
    EXPECT_EQ(left.op, ">=");

    // правая ветка — name != 'Bob'
    auto& right = *std::get<std::shared_ptr<BinaryExpr>>(top.right);
    EXPECT_EQ(right.op, "!=");
}

TEST(ParserTest, SelectWithOrCondition) {
    auto stmt = parseSQL(
        "SELECT * FROM users WHERE age < 18 OR age > 60;");
    auto& sel = std::get<SelectStatement>(stmt);

    EXPECT_TRUE(sel.where.has_value());
    auto& top = *std::get<std::shared_ptr<BinaryExpr>>(sel.where.value());
    EXPECT_EQ(top.op, "OR");
}

TEST(ParserTest, SelectWithLimit) {
    auto stmt = parseSQL("SELECT * FROM users LIMIT 5;");
    auto& sel = std::get<SelectStatement>(stmt);

    EXPECT_TRUE(sel.limit.has_value());
    EXPECT_EQ(sel.limit.value(), 5);
}

TEST(ParserTest, SelectWithOrderBy) {
    auto stmt = parseSQL("SELECT * FROM users ORDER BY name ASC;");
    auto& sel = std::get<SelectStatement>(stmt);

    EXPECT_EQ(sel.orderBy.size(), 1);
    EXPECT_EQ(sel.orderBy[0].column,    "name");
    EXPECT_TRUE(sel.orderBy[0].ascending);
}

TEST(ParserTest, SelectWithOrderByDesc) {
    auto stmt = parseSQL("SELECT * FROM users ORDER BY age DESC;");
    auto& sel = std::get<SelectStatement>(stmt);

    EXPECT_EQ(sel.orderBy.size(), 1);
    EXPECT_EQ(sel.orderBy[0].column,     "age");
    EXPECT_FALSE(sel.orderBy[0].ascending);
}

TEST(ParserTest, SelectFullQuery) {
    auto stmt = parseSQL(
        "SELECT id, name FROM users WHERE age >= 18 ORDER BY name ASC LIMIT 10;");
    auto& sel = std::get<SelectStatement>(stmt);

    EXPECT_EQ(sel.table,          "users");
    EXPECT_EQ(sel.columns.size(), 2);
    EXPECT_TRUE(sel.where.has_value());
    EXPECT_EQ(sel.orderBy.size(), 1);
    EXPECT_TRUE(sel.limit.has_value());
    EXPECT_EQ(sel.limit.value(),  10);
}

TEST(ParserTest, CaseInsensitive) {
    auto stmt = parseSQL("select * from users where age = 25;");
    auto& sel = std::get<SelectStatement>(stmt);
    EXPECT_EQ(sel.table, "users");
    EXPECT_TRUE(sel.where.has_value());
}

// ─────────────────────────────────────────────────────────────────
//  INSERT tests
// ─────────────────────────────────────────────────────────────────

TEST(ParserTest, InsertStatement) {
    auto stmt = parseSQL(
        "INSERT INTO users (id, name, age) VALUES (1, 'Alice', 30);");
    auto& ins = std::get<InsertStatement>(stmt);

    EXPECT_EQ(ins.table,          "users");
    EXPECT_EQ(ins.columns.size(), 3);
    EXPECT_EQ(ins.columns[0],     "id");
    EXPECT_EQ(ins.columns[1],     "name");
    EXPECT_EQ(ins.columns[2],     "age");
    EXPECT_EQ(ins.values.size(),  3);

    // первое значение — число 1
    auto& v0 = *std::get<std::shared_ptr<LiteralExpr>>(ins.values[0]);
    EXPECT_EQ(v0.value, "1");
    EXPECT_EQ(v0.kind,  LiteralExpr::Kind::Integer);

    // второе значение — строка 'Alice'
    auto& v1 = *std::get<std::shared_ptr<LiteralExpr>>(ins.values[1]);
    EXPECT_EQ(v1.value, "Alice");
    EXPECT_EQ(v1.kind,  LiteralExpr::Kind::String);
}

// ─────────────────────────────────────────────────────────────────
//  UPDATE tests
// ─────────────────────────────────────────────────────────────────

TEST(ParserTest, UpdateStatement) {
    auto stmt = parseSQL(
        "UPDATE users SET name = 'Bob', age = 25 WHERE id = 1;");
    auto& upd = std::get<UpdateStatement>(stmt);

    EXPECT_EQ(upd.table,              "users");
    EXPECT_EQ(upd.assignments.size(), 2);
    EXPECT_EQ(upd.assignments[0].first, "name");
    EXPECT_EQ(upd.assignments[1].first, "age");
    EXPECT_TRUE(upd.where.has_value());
}

TEST(ParserTest, UpdateWithoutWhere) {
    auto stmt = parseSQL("UPDATE users SET active = true;");
    auto& upd = std::get<UpdateStatement>(stmt);

    EXPECT_EQ(upd.table,              "users");
    EXPECT_EQ(upd.assignments.size(), 1);
    EXPECT_FALSE(upd.where.has_value());
}

// ─────────────────────────────────────────────────────────────────
//  DELETE tests
// ─────────────────────────────────────────────────────────────────

TEST(ParserTest, DeleteWithWhere) {
    auto stmt = parseSQL("DELETE FROM users WHERE id = 1;");
    auto& del = std::get<DeleteStatement>(stmt);

    EXPECT_EQ(del.table, "users");
    EXPECT_TRUE(del.where.has_value());

    auto& bin = *std::get<std::shared_ptr<BinaryExpr>>(del.where.value());
    EXPECT_EQ(bin.op, "=");
}

TEST(ParserTest, DeleteWithoutWhere) {
    auto stmt = parseSQL("DELETE FROM users;");
    auto& del = std::get<DeleteStatement>(stmt);

    EXPECT_EQ(del.table, "users");
    EXPECT_FALSE(del.where.has_value());
}

// ─────────────────────────────────────────────────────────────────
//  CREATE TABLE tests
// ─────────────────────────────────────────────────────────────────

TEST(ParserTest, CreateTable) {
    auto stmt = parseSQL(
        "CREATE TABLE users (id INT PRIMARY KEY, name TEXT NOT NULL, age INT);");
    auto& ct = std::get<CreateTableStatement>(stmt);

    EXPECT_EQ(ct.table,          "users");
    EXPECT_EQ(ct.columns.size(), 3);

    EXPECT_EQ(ct.columns[0].name,       "id");
    EXPECT_EQ(ct.columns[0].type,       "INT");
    EXPECT_TRUE(ct.columns[0].primaryKey);
    EXPECT_FALSE(ct.columns[0].notNull);

    EXPECT_EQ(ct.columns[1].name,       "name");
    EXPECT_EQ(ct.columns[1].type,       "TEXT");
    EXPECT_FALSE(ct.columns[1].primaryKey);
    EXPECT_TRUE(ct.columns[1].notNull);

    EXPECT_EQ(ct.columns[2].name,       "age");
    EXPECT_EQ(ct.columns[2].type,       "INT");
    EXPECT_FALSE(ct.columns[2].primaryKey);
    EXPECT_FALSE(ct.columns[2].notNull);
}

// ─────────────────────────────────────────────────────────────────
//  DROP TABLE tests
// ─────────────────────────────────────────────────────────────────

TEST(ParserTest, DropTable) {
    auto stmt = parseSQL("DROP TABLE users;");
    auto& dt  = std::get<DropTableStatement>(stmt);
    EXPECT_EQ(dt.table, "users");
}

// ─────────────────────────────────────────────────────────────────
//  Error handling tests
// ─────────────────────────────────────────────────────────────────

TEST(ParserTest, InvalidKeywordThrows) {
    EXPECT_THROW(parseSQL("BANANA FROM users;"), ParseError);
}

TEST(ParserTest, MissingFromThrows) {
    EXPECT_THROW(parseSQL("SELECT id users;"), ParseError);
}

TEST(ParserTest, MissingTableNameThrows) {
    EXPECT_THROW(parseSQL("SELECT * FROM;"), ParseError);
}

TEST(ParserTest, UnterminatedParenThrows) {
    EXPECT_THROW(parseSQL("SELECT * FROM users WHERE (age > 30;"), ParseError);
}