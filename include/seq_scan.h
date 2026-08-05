#pragma once
#include "operator.h"
#include "mvcc.h"
#include <string>
#include <vector>
#include <memory>

namespace mydb {

    class SeqScan : public Operator {
    public:
        // Legacy constructor — no MVCC filtering
        SeqScan(const std::string& tableName, const std::vector<Row>& rows)
            : tableName_(tableName)
            , rows_(&rows)
            , cursor_(0) {}

        // MVCC-aware constructor — filters by snapshot visibility
        SeqScan(const std::string& tableName,
                const std::vector<VersionedRow>& versionedRows,
                std::shared_ptr<MvccSnapshot> snapshot)
            : tableName_(tableName)
            , rows_(nullptr)
            , versionedRows_(versionedRows)
            , snapshot_(std::move(snapshot))
            , cursor_(0) {}

        void open()  override { cursor_ = 0; }
        void close() override {}

        std::optional<Row> next() override {
            if (snapshot_) {
                while (cursor_ < versionedRows_.size()) {
                    if (snapshot_->isVisible(versionedRows_[cursor_])) {
                        return versionedRows_[cursor_++].data;
                    }
                    ++cursor_;
                }
                return std::nullopt;
            }
            if (!rows_ || cursor_ >= rows_->size()) return std::nullopt;
            return (*rows_)[cursor_++];
        }

    private:
        std::string                     tableName_;
        const std::vector<Row>*         rows_ = nullptr;
        std::vector<VersionedRow>       versionedRows_;
        std::shared_ptr<MvccSnapshot>   snapshot_;
        size_t                          cursor_;
    };

} // namespace mydb
