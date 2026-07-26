#include "lock_manager.h"
#include <algorithm>

namespace mydb {

    bool LockManager::acquireLock(TxnId txnId,
                                   const std::string& resource,
                                   LockMode mode) {
        std::lock_guard<std::mutex> guard(mutex_);
        auto& entries = locks_[resource];

        for (const auto& entry : entries) {
            if (entry.holder == txnId) continue; // same transaction

            // Shared locks conflict only with exclusive locks
            if (mode == LockMode::Exclusive || entry.mode == LockMode::Exclusive)
                return false; // conflict — lock denied
        }

        entries.push_back({txnId, mode});
        return true;
    }

    void LockManager::releaseAll(TxnId txnId) {
        std::lock_guard<std::mutex> guard(mutex_);
        for (auto& [resource, entries] : locks_) {
            entries.erase(
                std::remove_if(entries.begin(), entries.end(),
                    [txnId](const LockEntry& e) { return e.holder == txnId; }),
                entries.end()
            );
        }
    }

    bool LockManager::isLocked(const std::string& resource) const {
        std::lock_guard<std::mutex> guard(mutex_);
        auto it = locks_.find(resource);
        return it != locks_.end() && !it->second.empty();
    }

} // namespace mydb
