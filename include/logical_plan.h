#pragam once
#include "ast.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace mydb {

// ─────────────────────────────────────────────────────────────────
// Logical Plan Node types
// Каждый узел = одна реляционная операция
// ─────────────────────────────────────────────────────────────────

enum class LogicalNodeType {
    Scan,        // читаем таблицу целиком
    Filter,      // фильтруем строки по условию WHERE
    Projection,  // выбираем только нужные колонки
    Sort,        // сортировка ORDER BY
    Limit,       // ограничение LIMIT
    Join         // соединение двух таблиц
};

// Базовый класс для всех узлов логического плана
struct LogicalNode {
    LogicalNodeType              type;
    std::vector<std::shared_ptr<LogicalNode>> children; // дочерние узлы

    explicit LogicalNode(LogicalNodeType t) : type(t) {}
    virtual ~LogicalNode() = default;
};

// ── Scan ─────────────────────────────────────────────────────────
// Читаем все строки из таблицы
// SELECT * FROM users  →  Scan("users")
struct LogicalScan : LogicalNode {
    std::string tableName;

    explicit LogicalScan(const std::string& table)
        : LogicalNode(LogicalNodeType::Scan), tableName(table) {}
};

// ── Filter ───────────────────────────────────────────────────────
// Фильтруем строки по условию
// WHERE age >= 30  →  Filter(age >= 30)
struct LogicalFilter : LogicalNode {
    Expr predicate; // условие из WHERE

    explicit LogicalFilter(Expr pred)
        : LogicalNode(LogicalNodeType::Filter)
        , predicate(std::move(pred)) {}
};

// ── Projection ───────────────────────────────────────────────────
// Выбираем только нужные колонки
// SELECT id, name  →  Projection([id, name])
struct LogicalProjection : LogicalNode {
    std::vector<std::string> columns; // ["id", "name"] или ["*"]

    explicit LogicalProjection(std::vector<std::string> cols)
        : LogicalNode(LogicalNodeType::Projection)
        , columns(std::move(cols)) {}
};

// ── Sort ─────────────────────────────────────────────────────────
// ORDER BY name ASC
struct LogicalSort : LogicalNode {
    std::vector<OrderByClause> orderBy;

    explicit LogicalSort(std::vector<OrderByClause> order)
        : LogicalNode(LogicalNodeType::Sort)
        , orderBy(std::move(order)) {}
};

// ── Limit ────────────────────────────────────────────────────────
// LIMIT 10
struct LogicalLimit : LogicalNode {
    int count;

    explicit LogicalLimit(int n)
        : LogicalNode(LogicalNodeType::Limit), count(n) {}
};

// Удобный псевдоним
using LogicalPlanRef = std::shared_ptr<LogicalNode>;

} // namespace mydb