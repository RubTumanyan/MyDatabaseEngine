#pragma once
#include "row.h"
#include "ast.h"
#include <vector>
#include <optional>
#include <memory>

namespace mydb {

    // Base class for all Volcano-model operators.
    // Every operator exposes exactly one method: next().
    // The caller keeps calling next() until std::nullopt is returned.
    class Operator {
    public:
        virtual ~Operator() = default;

        // Called once before any next() calls — lets operators initialize state
        virtual void open()  = 0;

        // Returns the next row, or nullopt when exhausted
        virtual std::optional<Row> next() = 0;

        // Called once after all rows are consumed — lets operators release resources
        virtual void close() = 0;
    };

    using OperatorRef = std::unique_ptr<Operator>;

} // namespace mydb