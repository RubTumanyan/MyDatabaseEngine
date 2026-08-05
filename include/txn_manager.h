#pragma once
#include "transaction.h"
#include "lock_manager.h"
#include "executor.h"
#include "wal.h"
#include <unordered_map>
#include <algorithm>
#include <stdexcept>

namespace mydb {

class TransactionManager {
public:
    TransactionManager() = default;
    explicit TransactionManager(WAL* wal) : wal_(wal) {}

    Transaction& begin() {
        TxnId id = nextId_++;
        transactions_.emplace(id, Transaction(id));

        if (wal_) {
            LogRecord rec;
            rec.txnId = id;
            rec.type  = LogRecord::Type::Begin;
            wal_->append(rec);
        }

        return transactions_.at(id);
    }

    void commit(TxnId id) {
        auto& txn = get(id);
        if (!txn.active())
            throw std::runtime_error("Transaction is not active");

        if (wal_) {
            for (const auto& wr : txn.writeLog()) {
                LogRecord rec;
                rec.txnId = id;
                rec.table = wr.table;
                rec.key   = wr.key;
                rec.oldValue = WAL::serializeRow(wr.oldRow);
                rec.newValue = WAL::serializeRow(wr.newRow);

                switch (wr.type) {
                    case WriteRecord::Type::Insert:
                        rec.type = LogRecord::Type::Insert; break;
                    case WriteRecord::Type::Update:
                        rec.type = LogRecord::Type::Update; break;
                    case WriteRecord::Type::Delete:
                        rec.type = LogRecord::Type::Delete; break;
                }
                wal_->append(rec);
            }

            LogRecord commitRec;
            commitRec.txnId = id;
            commitRec.type  = LogRecord::Type::Commit;
            wal_->append(commitRec);
            wal_->flush();
        }

        txn.commit();
        lockManager_.releaseAll(id);
    }

    void rollback(TxnId id, TableStorage& storage) {
        auto& txn = get(id);
        if (!txn.active())
            throw std::runtime_error("Transaction is not active");

        const auto& log = txn.writeLog();
        for (auto it = log.rbegin(); it != log.rend(); ++it) {
            auto& table = storage[it->table];

            if (it->type == WriteRecord::Type::Insert) {
                table.erase(
                    std::remove_if(table.begin(), table.end(),
                        [&](const Row& r) {
                            auto ki = r.find("id");
                            return ki != r.end() && ki->second == it->key;
                        }),
                    table.end()
                );
            } else if (it->type == WriteRecord::Type::Update ||
                       it->type == WriteRecord::Type::Delete) {
                for (auto& row : table) {
                    auto ki = row.find("id");
                    if (ki != row.end() && ki->second == it->key) {
                        row = it->oldRow;
                        break;
                    }
                }
            }
        }

        if (wal_) {
            for (const auto& wr : txn.writeLog()) {
                LogRecord rec;
                rec.txnId = id;
                rec.table = wr.table;
                rec.key   = wr.key;
                rec.oldValue = WAL::serializeRow(wr.oldRow);
                rec.newValue = WAL::serializeRow(wr.newRow);

                switch (wr.type) {
                    case WriteRecord::Type::Insert:
                        rec.type = LogRecord::Type::Insert; break;
                    case WriteRecord::Type::Update:
                        rec.type = LogRecord::Type::Update; break;
                    case WriteRecord::Type::Delete:
                        rec.type = LogRecord::Type::Delete; break;
                }
                wal_->append(rec);
            }

            LogRecord abortRec;
            abortRec.txnId = id;
            abortRec.type  = LogRecord::Type::Abort;
            wal_->append(abortRec);
            wal_->flush();
        }

        txn.abort();
        lockManager_.releaseAll(id);
    }

    Transaction& get(TxnId id) {
        auto it = transactions_.find(id);
        if (it == transactions_.end())
            throw std::runtime_error("Transaction not found");
        return it->second;
    }

    LockManager& lockManager() { return lockManager_; }

private:
    TxnId                                  nextId_ = 1;
    std::unordered_map<TxnId, Transaction> transactions_;
    LockManager                            lockManager_;
    WAL*                                   wal_ = nullptr;
};

} // namespace mydb
