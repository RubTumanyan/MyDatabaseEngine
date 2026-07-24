#pragma once
#include "operator.h"

namespace mydb {

    class Limit : public Operator {
    public:
        Limit(OperatorRef child, int count)
            : child_(std::move(child))
            , count_(count)
            , seen_(0) {}

        void open()  override { child_->open(); seen_ = 0; }
        void close() override { child_->close(); }

        std::optional<Row> next() override {
            if (seen_ >= count_) return std::nullopt;
            auto row = child_->next();
            if (!row.has_value()) return std::nullopt;
            ++seen_;
            return row;
        }

    private:
        OperatorRef child_;
        int         count_;
        int         seen_;
    };

}