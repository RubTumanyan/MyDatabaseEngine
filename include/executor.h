#pragma once
#include "operator.h"
#include "physical_plan.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace mydb {

    // Simple in-memory table storage — will be replaced by Buffer Pool in Step 6
    using TableStorage = std::unordered_map<std::string, std::vector<Row>>;

    // Executor walks the physical plan tree and builds an operator pipeline.
    // Then drives the pipeline by calling next() until exhausted.
    class Executor {
    public:
        explicit Executor(TableStorage& storage) : storage_(storage) {}

        // Execute a physical plan — returns all result rows
        std::vector<Row> execute(const PhysicalPlanRef& plan);

    private:
        TableStorage& storage_;

        // Recursively build operator tree from physical plan
        OperatorRef build(const PhysicalPlanRef& node);
    };

} // namespace mydb