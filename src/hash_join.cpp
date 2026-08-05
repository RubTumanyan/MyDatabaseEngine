#include "hash_join.h"

namespace mydb {

HashJoin::HashJoin(OperatorRef build, OperatorRef probe,
                   std::string buildKey, std::string probeKey)
    : build_(std::move(build))
    , probe_(std::move(probe))
    , buildKey_(std::move(buildKey))
    , probeKey_(std::move(probeKey)) {}

void HashJoin::open() {
    build_->open();
    probe_->open();

    hashTable_.clear();
    while (auto row = build_->next()) {
        auto it = row->find(buildKey_);
        if (it != row->end())
            hashTable_[it->second].push_back(*row);
    }

    probeCursor_ = 0;
    currentMatches_.clear();
}

void HashJoin::close() {
    build_->close();
    probe_->close();
}

std::optional<Row> HashJoin::next() {
    while (true) {
        while (probeCursor_ < currentMatches_.size()) {
            Row joined = probeRow_;
            for (const auto& [k, v] : currentMatches_[probeCursor_]) {
                if (joined.find(k) == joined.end())
                    joined[k] = v;
            }
            ++probeCursor_;
            return joined;
        }

        auto probeRow = probe_->next();
        if (!probeRow.has_value()) return std::nullopt;

        auto it = probeRow->find(probeKey_);
        if (it != probeRow->end()) {
            auto hit = hashTable_.find(it->second);
            if (hit != hashTable_.end()) {
                probeRow_ = *probeRow;
                currentMatches_ = hit->second;
                probeCursor_ = 0;
                continue;
            }
        }
    }
}

} // namespace mydb
