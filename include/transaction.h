#pragma once
#include "row.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <stdexcept>

namespace mydb {

    using TxnId = uint64_t;

    struct WriteRecord {
        enum class Type { Insert, Update, Delete };
        Type        type;
        std::string table;
        std::string key;
        Row         oldRow;
        Row         newRow;
    };

    enum class TxnStatus { Active, Committed, Aborted };

    class Transaction {
    public:
        explicit Transaction(TxnId id) : id_(id), status_(TxnStatus::Active) {}

        TxnId     id()     const { return id_; }
        TxnStatus status() const { return status_; }
        bool      active() const { return status_ == TxnStatus::Active; }

        void logWrite(WriteRecord record) {
            writeLog_.push_back(std::move(record));
        }

        const std::vector<WriteRecord>& writeLog() const { return writeLog_; }

        void commit() { status_ = TxnStatus::Committed; }
        void abort()  { status_ = TxnStatus::Aborted;   }

    private:
        TxnId                    id_;
        TxnStatus                status_;
        std::vector<WriteRecord> writeLog_;
    };

} // namespace mydb