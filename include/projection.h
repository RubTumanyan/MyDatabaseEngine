#pragma once
#include "operator.h"

namespace mydb {

    class Projection : public Operator {
    public:
        Projection(OperatorRef child, std::vector<std::string> columns)
            : child_(std::move(child))
            , columns_(std::move(columns)) {}

        void open()  override { child_->open(); }
        void close() override { child_->close(); }

        std::optional<Row> next() override {
            auto row = child_->next();
            if (!row.has_value()) return std::nullopt;

            if (columns_.size() == 1 && columns_[0] == "*")
                return row;

            Row projected;
            for (const auto& col : columns_) {
                auto it = row->find(col);
                if (it != row->end())
                    projected[col] = it->second;
            }
            return projected;
        }

    private:
        OperatorRef              child_;
        std::vector<std::string> columns_;
    };

} // namespace mydb