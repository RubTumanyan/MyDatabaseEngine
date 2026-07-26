#pragma once
#include "transaction.h"
#include "lock_manager.h"
#include "executor.h"
#include <unordered_map>
#include <algorithm>
#include <stdexcept>

namespace mydb {

class TransactionManager {
public:
    Transaction& begin() {
        TxnId id = nextId_++;
        transactions_.emplace(id, Transaction(id));
        return transactions_.at(id);
    }

    void commit(TxnId id) {
        auto& txn = get(id);
        if (!txn.active())
            throw std::runtime_error("Transaction is not active");
        txn.commit();
        lockManager_.releaseAll(id);
    }

    void rollback(TxnId id, TableStorage& storage) {
        auto& txn = get(id);
        if (!txn.active())
            throw std::runtime_error("Transaction is not active");

        // Replay writes in reverse — undo each operation
        const auto& log = txn.writeLog();
        for (auto it = log.rbegin(); it != log.rend(); ++it) {
            auto& table = storage[it->table];

            if (it->type == WriteRecord::Type::Insert) {
                // undo insert: remove the row we added
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
                // undo update/delete: restore old row
                for (auto& row : table) {
                    auto ki = row.find("id");
                    if (ki != row.end() && ki->second == it->key) {
                        row = it->oldRow;
                        break;
                    }
                }
            }
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
};

} // namespace mydb