#pragma once
#include "logical_plan.h"

namespace mydb {

    // Physical plan represents HOW to execute the logical plan.
    // Same tree shape, but each node picks a concrete algorithm.
    // Example: LogicalScan → PhysicalSeqScan (no index available)
    //          LogicalScan → PhysicalIndexScan (index exists — Step 7)
    enum class PhysicalNodeType {
        SeqScan,     // full table scan, O(n)
        Filter,
        Projection,
        Sort,        // in-memory sort, O(n log n)
        Limit,
        HashJoin     // build hash table on smaller side, O(n+m)
    };

    struct PhysicalNode {
        PhysicalNodeType                           type;
        std::vector<std::shared_ptr<PhysicalNode>> children;

        explicit PhysicalNode(PhysicalNodeType t) : type(t) {}
        virtual ~PhysicalNode() = default;
    };

    // No index — read every page sequentially
    struct PhysicalSeqScan : PhysicalNode {
        std::string tableName;
        explicit PhysicalSeqScan(const std::string& table)
            : PhysicalNode(PhysicalNodeType::SeqScan), tableName(table) {}
    };

    struct PhysicalFilter : PhysicalNode {
        Expr predicate;
        explicit PhysicalFilter(Expr pred)
            : PhysicalNode(PhysicalNodeType::Filter), predicate(std::move(pred)) {}
    };

    struct PhysicalProjection : PhysicalNode {
        std::vector<std::string> columns;
        explicit PhysicalProjection(std::vector<std::string> cols)
            : PhysicalNode(PhysicalNodeType::Projection), columns(std::move(cols)) {}
    };

    // Sort loads all rows into memory — only feasible for small result sets
    struct PhysicalSort : PhysicalNode {
        std::vector<OrderByClause> orderBy;
        explicit PhysicalSort(std::vector<OrderByClause> order)
            : PhysicalNode(PhysicalNodeType::Sort), orderBy(std::move(order)) {}
    };

    struct PhysicalLimit : PhysicalNode {
        int count;
        explicit PhysicalLimit(int n)
            : PhysicalNode(PhysicalNodeType::Limit), count(n) {}
    };

    using PhysicalPlanRef = std::shared_ptr<PhysicalNode>;

} // namespace mydb