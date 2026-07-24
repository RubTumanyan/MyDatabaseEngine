#pragma once
#include "logical_plan.h"
#include "physical_plan.h"
#include "ast.h"

namespace mydb {

    // Optimizer runs in two passes:
    //   Pass 1 — AST → LogicalPlan  (what operations are needed)
    //   Pass 2 — LogicalPlan → PhysicalPlan  (how to execute each operation)
    //
    // In a production DB, pass 2 would use statistics and cost models
    // to choose between e.g. SeqScan vs IndexScan, HashJoin vs MergeJoin.
    // We keep it simple for now — one physical algorithm per logical node.
    class Optimizer {
    public:
        LogicalPlanRef  toLogical (const Statement&      stmt);
        PhysicalPlanRef toPhysical(const LogicalPlanRef& logical);

    private:
        LogicalPlanRef buildSelect(const SelectStatement& stmt);
        LogicalPlanRef buildInsert(const InsertStatement& stmt);
        LogicalPlanRef buildDelete(const DeleteStatement& stmt);
        LogicalPlanRef buildUpdate(const UpdateStatement& stmt);

        PhysicalPlanRef buildPhysical(const LogicalPlanRef& node);
    };

} // namespace mydb