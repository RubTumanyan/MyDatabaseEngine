#pragma once
#include "transaction.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>

namespace mydb {

    enum class LockMode { Shared, Exclusive };

    struct LockEntry {
        TxnId    holder;
        LockMode mode;
    };

    class LockManager {
    public:
        bool acquireLock(TxnId txnId,
                         const std::string& resource,
                         LockMode mode);
        void releaseAll(TxnId txnId);
        bool isLocked(const std::string& resource) const;

    private:
        std::unordered_map<std::string, std::vector<LockEntry>> locks_;
        mutable std::mutex mutex_;
    };

} // namespace mydb