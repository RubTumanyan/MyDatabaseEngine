#include "optimizer.h"
#include <stdexcept>

namespace mydb {

// ── Pass 1: AST → Logical Plan ───────────────────────────────────

LogicalPlanRef Optimizer::toLogical(const Statement& stmt) {
    return std::visit([&](auto&& s) -> LogicalPlanRef {
        using T = std::decay_t<decltype(s)>;
        if      constexpr (std::is_same_v<T, SelectStatement>) return buildSelect(s);
        else if constexpr (std::is_same_v<T, InsertStatement>) return buildInsert(s);
        else if constexpr (std::is_same_v<T, DeleteStatement>) return buildDelete(s);
        else if constexpr (std::is_same_v<T, UpdateStatement>) return buildUpdate(s);
        else throw std::runtime_error("Optimizer: unsupported statement");
    }, stmt);
}

LogicalPlanRef Optimizer::buildSelect(const SelectStatement& stmt) {
    // Build bottom-up: the last node added becomes the tree root
    LogicalPlanRef cur = std::make_shared<LogicalScan>(stmt.table);

    // Filter before projection — avoids projecting rows we'll discard anyway
    if (stmt.where.has_value()) {
        auto f = std::make_shared<LogicalFilter>(stmt.where.value());
        f->children.push_back(cur);
        cur = f;
    }

    auto proj = std::make_shared<LogicalProjection>(stmt.columns);
    proj->children.push_back(cur);
    cur = proj;

    if (!stmt.orderBy.empty()) {
        auto sort = std::make_shared<LogicalSort>(stmt.orderBy);
        sort->children.push_back(cur);
        cur = sort;
    }

    // Limit sits at the top — executor stops pulling once count is reached
    if (stmt.limit.has_value()) {
        auto lim = std::make_shared<LogicalLimit>(stmt.limit.value());
        lim->children.push_back(cur);
        cur = lim;
    }

    return cur;
}

// INSERT/UPDATE/DELETE logical plans are stubs — executor handles them directly
LogicalPlanRef Optimizer::buildInsert(const InsertStatement& stmt) {
    return std::make_shared<LogicalScan>(stmt.table);
}

LogicalPlanRef Optimizer::buildDelete(const DeleteStatement& stmt) {
    auto scan = std::make_shared<LogicalScan>(stmt.table);
    if (!stmt.where.has_value()) return scan;

    // Push filter as low as possible so we skip rows early
    auto f = std::make_shared<LogicalFilter>(stmt.where.value());
    f->children.push_back(scan);
    return f;
}

LogicalPlanRef Optimizer::buildUpdate(const UpdateStatement& stmt) {
    auto scan = std::make_shared<LogicalScan>(stmt.table);
    if (!stmt.where.has_value()) return scan;

    auto f = std::make_shared<LogicalFilter>(stmt.where.value());
    f->children.push_back(scan);
    return f;
}

// ── Pass 2: Logical → Physical ───────────────────────────────────

PhysicalPlanRef Optimizer::toPhysical(const LogicalPlanRef& logical) {
    return buildPhysical(logical);
}

PhysicalPlanRef Optimizer::buildPhysical(const LogicalPlanRef& node) {
    switch (node->type) {

        case LogicalNodeType::Scan: {
            auto* n = static_cast<LogicalScan*>(node.get());
            // SeqScan is the only option until we add B-Tree indexes in Step 7
            return std::make_shared<PhysicalSeqScan>(n->tableName);
        }

        case LogicalNodeType::Filter: {
            auto* n   = static_cast<LogicalFilter*>(node.get());
            auto  out = std::make_shared<PhysicalFilter>(n->predicate);
            out->children.push_back(buildPhysical(node->children[0]));
            return out;
        }

        case LogicalNodeType::Projection: {
            auto* n   = static_cast<LogicalProjection*>(node.get());
            auto  out = std::make_shared<PhysicalProjection>(n->columns);
            out->children.push_back(buildPhysical(node->children[0]));
            return out;
        }

        case LogicalNodeType::Sort: {
            auto* n   = static_cast<LogicalSort*>(node.get());
            auto  out = std::make_shared<PhysicalSort>(n->orderBy);
            out->children.push_back(buildPhysical(node->children[0]));
            return out;
        }

        case LogicalNodeType::Limit: {
            auto* n   = static_cast<LogicalLimit*>(node.get());
            auto  out = std::make_shared<PhysicalLimit>(n->count);
            out->children.push_back(buildPhysical(node->children[0]));
            return out;
        }

        default:
            throw std::runtime_error("Optimizer: unknown node type");
    }
}

} // namespace mydb
