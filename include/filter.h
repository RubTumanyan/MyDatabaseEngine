#pragma once
#include "operator.h"

namespace mydb
{
    // Evaluates the WHERE predicate on each row from its child.
    // Rows that don't match are silently dropped.
    class Filter : public Operator {
    public:
        Filter(OperatorRef child, Expr predicate)
            : child_(std::move(child))
            , predicate_(std::move(predicate)) {}

        void open()  override { child_->open(); }
        void close() override { child_->close(); }

        std::optional<Row> next() override {
            // Keep pulling from child until we find a matching row
            while (true) {
                auto row = child_->next();
                if (!row.has_value()) return std::nullopt; // child exhausted

                if (evaluate(predicate_, *row))
                    return row;
                // else: row doesn't match — silently skip and try next
            }
        }

    private:
        OperatorRef child_;
        Expr        predicate_;

        // Recursively evaluate an expression against a row
        bool evaluate(const Expr& expr, const Row& row) const;

        // Compare two string values using a SQL operator
        bool compare(const std::string& left,
                     const std::string& op,
                     const std::string& right) const;

        // Resolve an Expr to its string value given a row
        std::string resolve(const Expr& expr, const Row& row) const;
    };
}