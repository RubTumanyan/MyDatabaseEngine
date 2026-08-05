#include "executor.h"
#include "seq_scan.h"
#include "filter.h"
#include "projection.h"
#include "limit.h"
#include "sort.h"
#include "hash_join.h"
#include "transaction.h"
#include <stdexcept>
#include <algorithm>

namespace mydb {

// ── Expression evaluation (used by Filter and DML) ───────────────

static std::string resolveExpr(const Expr& expr, const Row& row) {
    return std::visit([&](auto&& e) -> std::string {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, std::shared_ptr<LiteralExpr>>) {
            return e->value;
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ColumnRefExpr>>) {
            auto it = row.find(e->column);
            return (it != row.end()) ? it->second : "";
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<BinaryExpr>>) {
            return "";
        }
        else {
            return "";
        }
    }, expr);
}

static bool compareValues(const std::string& left,
                          const std::string& op,
                          const std::string& right) {
    try {
        double l = std::stod(left);
        double r = std::stod(right);
        if (op == "=")  return l == r;
        if (op == "!=") return l != r;
        if (op == "<")  return l <  r;
        if (op == ">")  return l >  r;
        if (op == "<=") return l <= r;
        if (op == ">=") return l >= r;
    } catch (...) {
        if (op == "=")  return left == right;
        if (op == "!=") return left != right;
        if (op == "<")  return left <  right;
        if (op == ">")  return left >  right;
        if (op == "<=") return left <= right;
        if (op == ">=") return left >= right;
    }
    return false;
}

static bool evaluateExpr(const Expr& expr, const Row& row) {
    return std::visit([&](auto&& e) -> bool {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, std::shared_ptr<BinaryExpr>>) {
            if (e->op == "AND")
                return evaluateExpr(e->left, row) && evaluateExpr(e->right, row);
            if (e->op == "OR")
                return evaluateExpr(e->left, row) || evaluateExpr(e->right, row);

            std::string left  = resolveExpr(e->left,  row);
            std::string right = resolveExpr(e->right, row);
            return compareValues(left, e->op, right);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<UnaryExpr>>) {
            if (e->op == "NOT")
                return !evaluateExpr(e->operand, row);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<LiteralExpr>>) {
            return e->value == "true" || e->value == "1";
        }
        return true;
    }, expr);
}

static std::string extractLiteral(const Expr& expr) {
    return std::visit([](auto&& e) -> std::string {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, std::shared_ptr<LiteralExpr>>)
            return e->value;
        return "";
    }, expr);
}

// ── Filter implementation ─────────────────────────────────────────

bool Filter::evaluate(const Expr& expr, const Row& row) const {
    return evaluateExpr(expr, row);
}

bool Filter::compare(const std::string& l,
                     const std::string& op,
                     const std::string& r) const {
    return compareValues(l, op, r);
}

std::string Filter::resolve(const Expr& expr, const Row& row) const {
    return resolveExpr(expr, row);
}

// ── Executor: operator tree builder ──────────────────────────────

OperatorRef Executor::build(const PhysicalPlanRef& node) {
    switch (node->type) {

        case PhysicalNodeType::SeqScan: {
            auto* n = static_cast<PhysicalSeqScan*>(node.get());

            auto it = storage_.find(n->tableName);
            if (it == storage_.end())
                throw std::runtime_error("Table not found: " + n->tableName);

            if (snapshot_) {
                std::vector<VersionedRow> versioned;
                for (const auto& row : it->second)
                    versioned.push_back({row, 1, 0});
                return std::make_unique<SeqScan>(
                    n->tableName, versioned, snapshot_);
            }

            return std::make_unique<SeqScan>(n->tableName, it->second);
        }

        case PhysicalNodeType::Filter: {
            auto* n    = static_cast<PhysicalFilter*>(node.get());
            auto  child = build(node->children[0]);
            return std::make_unique<Filter>(std::move(child), n->predicate);
        }

        case PhysicalNodeType::Projection: {
            auto* n    = static_cast<PhysicalProjection*>(node.get());
            auto  child = build(node->children[0]);
            return std::make_unique<Projection>(std::move(child), n->columns);
        }

        case PhysicalNodeType::Sort: {
            auto* n    = static_cast<PhysicalSort*>(node.get());
            auto  child = build(node->children[0]);
            return std::make_unique<Sort>(std::move(child), n->orderBy);
        }

        case PhysicalNodeType::Limit: {
            auto* n    = static_cast<PhysicalLimit*>(node.get());
            auto  child = build(node->children[0]);
            return std::make_unique<Limit>(std::move(child), n->count);
        }

        case PhysicalNodeType::HashJoin: {
            auto* n = static_cast<PhysicalHashJoin*>(node.get());
            auto buildChild = build(node->children[0]);
            auto probeChild = build(node->children[1]);
            return std::make_unique<HashJoin>(
                std::move(buildChild), std::move(probeChild),
                n->buildKey, n->probeKey);
        }

        default:
            throw std::runtime_error("Executor: unsupported operator type");
    }
}

// ── Executor: drive the pipeline ─────────────────────────────────

std::vector<Row> Executor::execute(const PhysicalPlanRef& plan) {
    auto op = build(plan);
    op->open();

    std::vector<Row> results;
    while (true) {
        auto row = op->next();
        if (!row.has_value()) break;
        results.push_back(*row);
    }

    op->close();
    return results;
}

// ── DML: INSERT ──────────────────────────────────────────────────

void Executor::executeInsert(const InsertStatement& stmt,
                             Transaction* txn) {
    if (!storage_.count(stmt.table))
        throw std::runtime_error("Table not found: " + stmt.table);

    Row row;
    for (size_t i = 0; i < stmt.columns.size(); ++i)
        row[stmt.columns[i]] = extractLiteral(stmt.values[i]);

    if (txn && txn->active()) {
        WriteRecord rec;
        rec.type   = WriteRecord::Type::Insert;
        rec.table  = stmt.table;
        rec.key    = row.count("id") ? row.at("id") : "";
        rec.newRow = row;
        txn->logWrite(rec);
    }

    storage_[stmt.table].push_back(std::move(row));
}

// ── DML: UPDATE ──────────────────────────────────────────────────

void Executor::executeUpdate(const UpdateStatement& stmt,
                             Transaction* txn) {
    auto it = storage_.find(stmt.table);
    if (it == storage_.end())
        throw std::runtime_error("Table not found: " + stmt.table);

    auto& table = it->second;
    for (auto& row : table) {
        bool matches = !stmt.where.has_value() ||
                       evaluateExpr(stmt.where.value(), row);
        if (!matches) continue;

        Row oldRow = row;

        for (const auto& [col, valExpr] : stmt.assignments)
            row[col] = resolveExpr(valExpr, row);

        if (txn && txn->active()) {
            WriteRecord rec;
            rec.type   = WriteRecord::Type::Update;
            rec.table  = stmt.table;
            rec.key    = row.count("id") ? row.at("id") : "";
            rec.oldRow = oldRow;
            rec.newRow = row;
            txn->logWrite(rec);
        }
    }
}

// ── DML: DELETE ──────────────────────────────────────────────────

void Executor::executeDelete(const DeleteStatement& stmt,
                             Transaction* txn) {
    auto it = storage_.find(stmt.table);
    if (it == storage_.end())
        throw std::runtime_error("Table not found: " + stmt.table);

    auto& table = it->second;
    table.erase(
        std::remove_if(table.begin(), table.end(),
            [&](const Row& row) {
                bool matches = !stmt.where.has_value() ||
                               evaluateExpr(stmt.where.value(), row);

                if (matches && txn && txn->active()) {
                    WriteRecord rec;
                    rec.type   = WriteRecord::Type::Delete;
                    rec.table  = stmt.table;
                    rec.key    = row.count("id") ? row.at("id") : "";
                    rec.oldRow = row;
                    txn->logWrite(rec);
                }

                return matches;
            }),
        table.end());
}

// ── DML: CREATE TABLE ────────────────────────────────────────────

void Executor::executeCreateTable(const CreateTableStatement& stmt) {
    if (storage_.count(stmt.table))
        throw std::runtime_error("Table already exists: " + stmt.table);
    storage_[stmt.table] = {};
}

} // namespace mydb
