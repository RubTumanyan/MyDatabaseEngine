#pragma once
#include "transaction.h"
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

namespace mydb {

using LSN = uint64_t; // Log Sequence Number — уникальный номер каждой записи в логе

// Every change is recorded as a log record before touching data
struct LogRecord {
    enum class Type : uint8_t {
        Begin,    // START of a transaction
        Commit,   // COMMIT — all changes are permanent
        Abort,    // ROLLBACK — all changes must be undone
        Update,   // a row was modified
        Insert,   // a row was inserted
        Delete    // a row was deleted
    };

    LSN         lsn;       // unique sequence number
    LSN         prevLsn;   // previous log record of same transaction (chain)
    TxnId       txnId;
    Type        type;
    std::string table;
    std::string key;
    std::string oldValue;  // serialized old row (for undo)
    std::string newValue;  // serialized new row (for redo)
};

// WAL writes log records to disk BEFORE modifying any page.
// On crash recovery, we replay the log to restore consistency.
class WAL {
public:
    explicit WAL(const std::string& logFilename);
    ~WAL();

    // Append a log record — returns its LSN
    LSN append(LogRecord record);

    // Force all buffered log records to disk (called before COMMIT)
    void flush();

    // Read all log records — used during crash recovery
    std::vector<LogRecord> readAll();

    // Recovery: redo all committed transactions,
    //           undo all incomplete transactions
    void recover(std::unordered_map<std::string,
                 std::vector<Row>>& storage);

    LSN nextLSN() const { return nextLsn_; }

private:
    std::string  filename_;
    std::fstream file_;
    LSN          nextLsn_ = 1;

    // Serialize/deserialize a LogRecord to/from binary
    void        writeRecord(const LogRecord& rec);
    bool        readRecord(LogRecord& rec);

    // Simple row serialization: "col1=val1;col2=val2"
    static std::string serializeRow(const Row& row);
    static Row         deserializeRow(const std::string& s);
};

} // namespace mydb