#pragma once
#include "operator.h"
#include "ast.h"
#include <vector>
#include <algorithm>

namespace mydb {

class Sort : public Operator {
public:
    Sort(OperatorRef child, std::vector<OrderByClause> orderBy)
        : child_(std::move(child))
        , orderBy_(std::move(orderBy)) {}

    void open() override {
        child_->open();
        buffered_.clear();
        while (auto row = child_->next())
            buffered_.push_back(std::move(*row));
        sortRows();
        cursor_ = 0;
    }

    void close() override { child_->close(); }

    std::optional<Row> next() override {
        if (cursor_ >= buffered_.size()) return std::nullopt;
        return buffered_[cursor_++];
    }

private:
    OperatorRef                child_;
    std::vector<OrderByClause> orderBy_;
    std::vector<Row>           buffered_;
    size_t                     cursor_ = 0;

    void sortRows() {
        std::sort(buffered_.begin(), buffered_.end(),
            [this](const Row& a, const Row& b) {
                for (const auto& clause : orderBy_) {
                    auto itA = a.find(clause.column);
                    auto itB = b.find(clause.column);
                    std::string valA = (itA != a.end()) ? itA->second : "";
                    std::string valB = (itB != b.end()) ? itB->second : "";
                    int cmp = compareValues(valA, valB);
                    if (cmp != 0)
                        return clause.ascending ? (cmp < 0) : (cmp > 0);
                }
                return false;
            });
    }

    static int compareValues(const std::string& a, const std::string& b) {
        try {
            double da = std::stod(a);
            double db = std::stod(b);
            if (da < db) return -1;
            if (da > db) return 1;
            return 0;
        } catch (...) {
            if (a < b) return -1;
            if (a > b) return 1;
            return 0;
        }
    }
};

} // namespace mydb
