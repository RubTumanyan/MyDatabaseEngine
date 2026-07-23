#pragma once
#include "logical_plan.h"
#include <string>
#include <vector>
#include <memory>

namespace mydb {

// ─────────────────────────────────────────────────────────────────
// Physical Plan Node types
// Логический план говорит ЧТО делать
// Физический план говорит КАК делать — конкретный алгоритм
// ─────────────────────────────────────────────────────────────────

enum class PhysicalNodeType {
    SeqScan,     // Sequential Scan — читаем таблицу строка за строкой
    Filter,      // применяем условие к каждой строке
    Projection,  // оставляем только нужные колонки
    Sort,        // сортировка (в памяти)
    Limit,       // останавливаемся после N строк
    HashJoin     // соединение через хеш-таблицу
};

// Базовый класс физического узла
struct PhysicalNode {
    PhysicalNodeType                          type;
    std::vector<std::shared_ptr<PhysicalNode>> children;

    explicit PhysicalNode(PhysicalNodeType t) : type(t) {}
    virtual ~PhysicalNode() = default;
};

// ── SeqScan ──────────────────────────────────────────────────────
// Полное сканирование таблицы строка за строкой
// Самый простой но медленный способ читать данные
struct PhysicalSeqScan : PhysicalNode {
    std::string tableName;

    explicit PhysicalSeqScan(const std::string& table)
        : PhysicalNode(PhysicalNodeType::SeqScan), tableName(table) {}
};

// ── Filter ───────────────────────────────────────────────────────
// Проверяем каждую строку — подходит ли она под условие
struct PhysicalFilter : PhysicalNode {
    Expr predicate;

    explicit PhysicalFilter(Expr pred)
        : PhysicalNode(PhysicalNodeType::Filter)
        , predicate(std::move(pred)) {}
};

// ── Projection ───────────────────────────────────────────────────
// Оставляем только нужные колонки из каждой строки
struct PhysicalProjection : PhysicalNode {
    std::vector<std::string> columns;

    explicit PhysicalProjection(std::vector<std::string> cols)
        : PhysicalNode(PhysicalNodeType::Projection)
        , columns(std::move(cols)) {}
};

// ── Sort ─────────────────────────────────────────────────────────
// Сортируем все строки в памяти
struct PhysicalSort : PhysicalNode {
    std::vector<OrderByClause> orderBy;

    explicit PhysicalSort(std::vector<OrderByClause> order)
        : PhysicalNode(PhysicalNodeType::Sort)
        , orderBy(std::move(order)) {}
};

// ── Limit ────────────────────────────────────────────────────────
// Останавливаемся после N строк
struct PhysicalLimit : PhysicalNode {
    int count;

    explicit PhysicalLimit(int n)
        : PhysicalNode(PhysicalNodeType::Limit), count(n) {}
};

// Удобный псевдоним
using PhysicalPlanRef = std::shared_ptr<PhysicalNode>;

} // namespace mydb