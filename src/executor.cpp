#include "executor.h"
#include "seq_scan.h"
#include "filter.h"
#include "projection.h"
#include "limit.h"
#include <stdexcept>
#include <algorithm>

namespace mydb {

// ── Expression evaluation (used by Filter) ────────────────────────

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
            // AND / OR handled separately in evaluate()
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
    // Try numeric comparison first
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
        // Fall back to string comparison
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
            // Logical operators recurse on both sides
            if (e->op == "AND")
                return evaluateExpr(e->left, row) && evaluateExpr(e->right, row);
            if (e->op == "OR")
                return evaluateExpr(e->left, row) || evaluateExpr(e->right, row);

            // Comparison operators resolve both sides to strings then compare
            std::string left  = resolveExpr(e->left,  row);
            std::string right = resolveExpr(e->right, row);
            return compareValues(left, e->op, right);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<UnaryExpr>>) {
            if (e->op == "NOT")
                return !evaluateExpr(e->operand, row);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<LiteralExpr>>) {
            // Bare literal used as bool: "true"/"false"
            return e->value == "true" || e->value == "1";
        }
        return true;
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

// ── Executor ──────────────────────────────────────────────────────

// Recursively builds operator pipeline from physical plan tree
OperatorRef Executor::build(const PhysicalPlanRef& node) {
    switch (node->type) {

        case PhysicalNodeType::SeqScan: {
            auto* n = static_cast<PhysicalSeqScan*>(node.get());

            // Look up table in storage — throw if not found
            auto it = storage_.find(n->tableName);
            if (it == storage_.end())
                throw std::runtime_error("Table not found: " + n->tableName);

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

        case PhysicalNodeType::Limit: {
            auto* n    = static_cast<PhysicalLimit*>(node.get());
            auto  child = build(node->children[0]);
            return std::make_unique<Limit>(std::move(child), n->count);
        }

        default:
            throw std::runtime_error("Executor: unsupported operator type");
    }
}

// Drive the pipeline — pull rows until exhausted
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

} // namespace mydb