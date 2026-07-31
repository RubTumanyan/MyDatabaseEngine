#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "optimizer.h"
#include "executor.h"
#include "txn_manager.h"

// ── Simple in-memory B-Tree test (no disk I/O) ────────────────────
// We test the B-Tree logic using a simple sorted map instead
#include <map>
#include <optional>
#include <vector>

// Simple in-memory index to demonstrate B-Tree concept
class SimpleIndex {
public:
    void insert(const std::string& key, const std::string& value) {
        data_[key] = value;
    }

    std::optional<std::string> search(const std::string& key) {
        auto it = data_.find(key);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<std::pair<std::string,std::string>>
    rangeScan(const std::string& from, const std::string& to) {
        std::vector<std::pair<std::string,std::string>> result;
        auto it = data_.lower_bound(from);
        while (it != data_.end() && it->first <= to) {
            result.emplace_back(it->first, it->second);
            ++it;
        }
        return result;
    }

private:
    std::map<std::string, std::string> data_;
};

int main() {
    // ── Full SQL pipeline test ────────────────────────────────────
    std::cout << "=== Full SQL Pipeline ===\n\n";

    mydb::TableStorage storage;
    storage["users"] = {
        {{"id","1"}, {"name","Alice"}, {"age","35"}},
        {{"id","2"}, {"name","Bob"},   {"age","22"}},
        {{"id","3"}, {"name","Carol"}, {"age","31"}},
        {{"id","4"}, {"name","Dave"},  {"age","17"}},
        {{"id","5"}, {"name","Eve"},   {"age","45"}},
    };

    auto runSQL = [&](const std::string& sql) {
        std::cout << "SQL: " << sql << "\n";
        mydb::Lexer     lexer(sql);
        mydb::Parser    parser(lexer.tokenize());
        mydb::Optimizer optimizer;
        mydb::Executor  executor(storage);

        auto stmt     = parser.parse();
        auto logical  = optimizer.toLogical(stmt);
        auto physical = optimizer.toPhysical(logical);
        auto results  = executor.execute(physical);

        std::cout << "Results (" << results.size() << " rows):\n";
        for (const auto& row : results) {
            for (const auto& [col, val] : row)
                std::cout << "  " << col << "=" << val;
            std::cout << "\n";
        }
        std::cout << "\n";
    };

    runSQL("SELECT id, name FROM users WHERE age >= 30;");
    runSQL("SELECT * FROM users WHERE age >= 18 LIMIT 3;");

    // ── Transaction test ──────────────────────────────────────────
    std::cout << "=== Transaction Test ===\n\n";

    mydb::TransactionManager txnMgr;

    // Test 1: commit
    {
        auto& txn = txnMgr.begin();
        std::cout << "BEGIN txn " << txn.id() << "\n";

        mydb::WriteRecord rec;
        rec.type   = mydb::WriteRecord::Type::Insert;
        rec.table  = "users";
        rec.key    = "6";
        rec.newRow = {{"id","6"}, {"name","Frank"}, {"age","28"}};
        txn.logWrite(rec);
        storage["users"].push_back(rec.newRow);

        txnMgr.commit(txn.id());
        std::cout << "COMMIT txn " << txn.id() << "\n";
        std::cout << "Rows after commit: " << storage["users"].size() << "\n\n";
    }

    // Test 2: rollback
    {
        auto& txn = txnMgr.begin();
        std::cout << "BEGIN txn " << txn.id() << "\n";

        mydb::WriteRecord rec;
        rec.type   = mydb::WriteRecord::Type::Insert;
        rec.table  = "users";
        rec.key    = "99";
        rec.newRow = {{"id","99"}, {"name","Temp"}, {"age","0"}};
        txn.logWrite(rec);
        storage["users"].push_back(rec.newRow);

        std::cout << "Rows before rollback: " << storage["users"].size() << "\n";
        txnMgr.rollback(txn.id(), storage);
        std::cout << "ROLLBACK txn " << txn.id() << "\n";
        std::cout << "Rows after rollback: " << storage["users"].size() << "\n\n";
    }

    // ── Index test (in-memory) ────────────────────────────────────
    std::cout << "=== Index Test (B-Tree concept) ===\n\n";

    SimpleIndex idx;
    idx.insert("005", "Alice");
    idx.insert("003", "Carol");
    idx.insert("007", "Bob");
    idx.insert("001", "Dave");
    idx.insert("009", "Eve");
    idx.insert("002", "Frank");
    idx.insert("006", "Grace");
    idx.insert("004", "Heidi");

    std::cout << "Search '005': "
              << idx.search("005").value_or("NOT FOUND") << "\n";
    std::cout << "Search '003': "
              << idx.search("003").value_or("NOT FOUND") << "\n";
    std::cout << "Search '999': "
              << idx.search("999").value_or("NOT FOUND") << "\n\n";

    std::cout << "Range scan '003' to '007':\n";
    for (auto& [k, v] : idx.rangeScan("003", "007"))
        std::cout << "  " << k << " -> " << v << "\n";

    // ── Lock Manager test ─────────────────────────────────────────
    std::cout << "\n=== Lock Manager Test ===\n\n";

    mydb::LockManager lm;
    bool ok1 = lm.acquireLock(1, "users:1", mydb::LockMode::Shared);
    bool ok2 = lm.acquireLock(2, "users:1", mydb::LockMode::Shared);
    bool ok3 = lm.acquireLock(3, "users:1", mydb::LockMode::Exclusive);

    std::cout << "Txn1 shared:    " << (ok1 ? "granted" : "denied") << "\n";
    std::cout << "Txn2 shared:    " << (ok2 ? "granted" : "denied") << "\n";
    std::cout << "Txn3 exclusive: " << (ok3 ? "granted" : "denied") << "\n";

    lm.releaseAll(1);
    lm.releaseAll(2);
    bool ok4 = lm.acquireLock(3, "users:1", mydb::LockMode::Exclusive);
    std::cout << "Txn3 exclusive after release: "
              << (ok4 ? "granted" : "denied") << "\n";

    return 0;
}