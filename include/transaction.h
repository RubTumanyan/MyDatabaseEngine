#pragma once
#include "row.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace mydb {

    using TxnId = uint64_t;

    // Each write operation is recorded as a log entry for rollback
    struct WriteRecord {
        enum class Type { Insert, Update, Delete };

        Type        type;
        std::string table;
        std::string key;        // primary key value
        Row         oldRow;     // state before the write (for rollback)
        Row         newRow;     // state after the write
    };

    enum class TxnStatus {
        Active,
        Committed,
        Aborted
    };

    // A single database transaction — tracks all writes for rollback
    class Transaction {
    public:
        explicit Transaction(TxnId id) : id_(id), status_(TxnStatus::Active) {}

        TxnId     id()     const { return id_; }
        TxnStatus status() const { return status_; }
        bool      active() const { return status_ == TxnStatus::Active; }

        // Called by executor to log every write before it happens
        void logWrite(WriteRecord record) {
            writeLog_.push_back(std::move(record));
        }

        const std::vector<WriteRecord>& writeLog() const { return writeLog_; }

        void commit()  { status_ = TxnStatus::Committed; }
        void abort()   { status_ = TxnStatus::Aborted;   }

    private:
        TxnId                    id_;
        TxnStatus                status_;
        std::vector<WriteRecord> writeLog_; // ordered log of all writes
    };

} // namespace mydb