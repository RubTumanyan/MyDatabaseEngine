#include <unordered_set>
#include "wal.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace mydb {

WAL::WAL(const std::string& filename) : filename_(filename) {
    file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        file_.open(filename, std::ios::in | std::ios::out |
                             std::ios::binary | std::ios::trunc);
    }
    if (!file_.is_open())
        throw std::runtime_error("Cannot open WAL file: " + filename);

    // Count existing records to set nextLsn_
    LogRecord rec;
    while (readRecord(rec)) nextLsn_ = rec.lsn + 1;
}

WAL::~WAL() {
    if (file_.is_open()) file_.close();
}

// ── Serialization helpers ─────────────────────────────────────────

std::string WAL::serializeRow(const Row& row) {
    std::string result;
    for (const auto& [col, val] : row)
        result += col + "=" + val + ";";
    return result;
}

Row WAL::deserializeRow(const std::string& s)
{
    Row row;
    std::string token;
    size_t start = 0;

    while (start < s.size()) {
        size_t end = s.find(';', start);
        if (end == std::string::npos) break;

        token = s.substr(start, end - start);
        start = end + 1;

        if (token.empty()) continue;

        auto eq = token.find('=');
        if (eq != std::string::npos) {
            std::string col = token.substr(0, eq);
            std::string val = token.substr(eq + 1);
            if (!col.empty())
                row[col] = val;
        }
    }
    return row;
}

void WAL::writeRecord(const LogRecord& rec) {
    auto writeStr = [&](const std::string& s) {
        uint32_t len = static_cast<uint32_t>(s.size());
        file_.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file_.write(s.data(), len);
    };

    file_.write(reinterpret_cast<const char*>(&rec.lsn),     sizeof(rec.lsn));
    file_.write(reinterpret_cast<const char*>(&rec.prevLsn), sizeof(rec.prevLsn));
    file_.write(reinterpret_cast<const char*>(&rec.txnId),   sizeof(rec.txnId));
    auto type = static_cast<uint8_t>(rec.type);
    file_.write(reinterpret_cast<const char*>(&type),        sizeof(type));

    writeStr(rec.table);
    writeStr(rec.key);
    writeStr(rec.oldValue);
    writeStr(rec.newValue);
}

bool WAL::readRecord(LogRecord& rec) {
    auto readStr = [&](std::string& s) -> bool {
        uint32_t len = 0;
        if (!file_.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
        s.resize(len);
        return static_cast<bool>(file_.read(s.data(), len));
    };

    if (!file_.read(reinterpret_cast<char*>(&rec.lsn),     sizeof(rec.lsn)))
        return false;
    if (!file_.read(reinterpret_cast<char*>(&rec.prevLsn), sizeof(rec.prevLsn)))
        return false;
    if (!file_.read(reinterpret_cast<char*>(&rec.txnId),   sizeof(rec.txnId)))
        return false;

    uint8_t type = 0;
    if (!file_.read(reinterpret_cast<char*>(&type), sizeof(type))) return false;
    rec.type = static_cast<LogRecord::Type>(type);

    return readStr(rec.table) && readStr(rec.key) &&
           readStr(rec.oldValue) && readStr(rec.newValue);
}

// ── Public API ────────────────────────────────────────────────────

LSN WAL::append(LogRecord record) {
    record.lsn = nextLsn_++;

    // Seek to end and write
    file_.seekp(0, std::ios::end);
    writeRecord(record);

    return record.lsn;
}

void WAL::flush() {
    file_.flush();
}

std::vector<LogRecord> WAL::readAll() {
    std::vector<LogRecord> records;
    file_.seekg(0, std::ios::beg);
    file_.clear();

    LogRecord rec;
    while (readRecord(rec))
        records.push_back(rec);

    return records;
}

void WAL::recover(std::unordered_map<std::string, std::vector<Row>>& storage) {
    auto records = readAll();

    // Find all committed transactions
    std::unordered_set<TxnId> committed;
    for (const auto& rec : records) {
        if (rec.type == LogRecord::Type::Commit)
            committed.insert(rec.txnId);
    }

    // REDO: replay committed transactions forward
    for (const auto& rec : records) {
        if (!committed.count(rec.txnId)) continue;

        if (rec.type == LogRecord::Type::Insert) {
            Row newRow = deserializeRow(rec.newValue);
            // avoid duplicate rows on multiple recoveries
            bool exists = false;
            for (const auto& r : storage[rec.table]) {
                auto it = r.find("id");
                if (it != r.end() && it->second == rec.key) {
                    exists = true; break;
                }
            }
            if (!exists) storage[rec.table].push_back(newRow);
        }
        else if (rec.type == LogRecord::Type::Update) {
            Row newRow = deserializeRow(rec.newValue);
            bool found = false;
            for (auto& row : storage[rec.table]) {
                if (row["id"] == rec.key) { row = newRow; found = true; break; }
            }
            if (!found) storage[rec.table].push_back(newRow);
        }
        else if (rec.type == LogRecord::Type::Delete) {
            auto& tbl = storage[rec.table];
            tbl.erase(std::remove_if(tbl.begin(), tbl.end(),
                [&](const Row& r) {
                    auto it = r.find("id");
                    return it != r.end() && it->second == rec.key;
                }), tbl.end());
        }
    }

    // UNDO: rollback incomplete transactions in reverse order
    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        if (committed.count(it->txnId)) continue;

        if (it->type == LogRecord::Type::Insert) {
            auto& tbl = storage[it->table];
            tbl.erase(std::remove_if(tbl.begin(), tbl.end(),
                [&](const Row& r) {
                    auto ki = r.find("id");
                    return ki != r.end() && ki->second == it->key;
                }), tbl.end());
        }
        else if (it->type == LogRecord::Type::Update ||
                 it->type == LogRecord::Type::Delete) {
            Row oldRow = deserializeRow(it->oldValue);
            for (auto& row : storage[it->table]) {
                if (row["id"] == it->key) { row = oldRow; break; }
            }
        }
    }
}
}