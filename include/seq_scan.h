#pragma once
#include "operator.h"
#include <string>
#include <vector>

namespace mydb {

    class SeqScan : public Operator {
    public:
        SeqScan(const std::string& tableName, const std::vector<Row>& rows)
            : tableName_(tableName)
            , rows_(rows)
            , cursor_(0) {}

        void open()  override { cursor_ = 0; }
        void close() override {}

        std::optional<Row> next() override {
            if (cursor_ >= rows_.size()) return std::nullopt;
            return rows_[cursor_++];
        }

    private:
        std::string             tableName_;
        const std::vector<Row>& rows_;
        size_t                  cursor_;
    };

} // namespace mydbnamespace mydb