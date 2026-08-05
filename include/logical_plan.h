#pragma once
#include "ast.h"
#include <string>
#include <vector>
#include <memory>

namespace mydb {

    // Logical plan represents WHAT to do — not HOW.
    // Each node is one relational algebra operation.
    // The tree is built bottom-up: Scan → Filter → Projection → Sort → Limit
    enum class LogicalNodeType {
        Scan,
        Filter,
        Projection,
        Sort,
        Limit,
        Join
    };

    struct LogicalNode {
        LogicalNodeType                           type;
        std::vector<std::shared_ptr<LogicalNode>> children;

        explicit LogicalNode(LogicalNodeType t) : type(t) {}
        virtual ~LogicalNode() = default;
    };

    // Always the bottom of the tree — reads every row from a table
    struct LogicalScan : LogicalNode {
        std::string tableName;
        explicit LogicalScan(const std::string& table)
            : LogicalNode(LogicalNodeType::Scan), tableName(table) {}
    };

    // Keeps only rows that satisfy the predicate
    struct LogicalFilter : LogicalNode {
        Expr predicate;
        explicit LogicalFilter(Expr pred)
            : LogicalNode(LogicalNodeType::Filter), predicate(std::move(pred)) {}
    };

    // Drops columns the user didn't ask for
    struct LogicalProjection : LogicalNode {
        std::vector<std::string> columns;
        explicit LogicalProjection(std::vector<std::string> cols)
            : LogicalNode(LogicalNodeType::Projection), columns(std::move(cols)) {}
    };

    struct LogicalSort : LogicalNode {
        std::vector<OrderByClause> orderBy;
        explicit LogicalSort(std::vector<OrderByClause> order)
            : LogicalNode(LogicalNodeType::Sort), orderBy(std::move(order)) {}
    };

    // Stops pulling rows once count is reached — avoids scanning the whole table
    struct LogicalLimit : LogicalNode {
        int count;
        explicit LogicalLimit(int n)
            : LogicalNode(LogicalNodeType::Limit), count(n) {}
    };

    struct LogicalJoin : LogicalNode {
        std::string leftKey;
        std::string rightKey;
        explicit LogicalJoin(std::string lk, std::string rk)
            : LogicalNode(LogicalNodeType::Join),
              leftKey(std::move(lk)), rightKey(std::move(rk)) {}
    };

    using LogicalPlanRef = std::shared_ptr<LogicalNode>;

} // namespace mydb