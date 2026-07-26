#pragma once
#include "transaction.h"
#include <unordered_map>
#include <mutex>
#include <string>

namespace mydb {

    enum class LockMode {
        Shared,    // read lock  — multiple transactions can hold simultaneously
        Exclusive  // write lock — only one transaction can hold at a time
    };

    // Tracks which transaction holds which lock on which resource
    struct LockEntry {
        TxnId    holder;
        LockMode mode;
    };

    // Simple lock manager — detects conflicts, grants or denies locks
    // In a real DB this would also handle deadlock detection
    class LockManager {
    public:
        // Returns true if lock was granted, false if conflict
        bool acquireLock(TxnId txnId,
                         const std::string& resource,
                         LockMode mode);

        // Release all locks held by this transaction
        void releaseAll(TxnId txnId);

        bool isLocked(const std::string& resource) const;

    private:
        // resource → list of lock entries
        std::unordered_map<std::string, std::vector<LockEntry>> locks_;
        mutable std::mutex mutex_;
    };

} // namespace mydb