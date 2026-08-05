#pragma once
#include "operator.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace mydb {

class HashJoin : public Operator {
public:
    HashJoin(OperatorRef build, OperatorRef probe,
             std::string buildKey, std::string probeKey);

    void open() override;
    void close() override;
    std::optional<Row> next() override;

private:
    OperatorRef build_;
    OperatorRef probe_;
    std::string buildKey_;
    std::string probeKey_;

    std::unordered_map<std::string, std::vector<Row>> hashTable_;
    std::vector<Row> currentMatches_;
    Row probeRow_;
    size_t probeCursor_ = 0;
};

} // namespace mydb
