#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "optimizer.h"
#include "executor.h"
#include "txn_manager.h"   // ← вместо transaction.h

int main() {
    mydb::TableStorage       storage;
    mydb::TransactionManager txnMgr;

    storage["accounts"] = {
        {{"id","1"}, {"name","Alice"}, {"balance","1000"}},
        {{"id","2"}, {"name","Bob"},   {"balance","500"}},
    };

    std::cout << "=== Test 1: Successful COMMIT ===\n";
    {
        auto& txn = txnMgr.begin();
        std::cout << "BEGIN txn " << txn.id() << "\n";

        mydb::WriteRecord rec;
        rec.type   = mydb::WriteRecord::Type::Insert;
        rec.table  = "accounts";
        rec.key    = "3";
        rec.newRow = {{"id","3"}, {"name","Carol"}, {"balance","750"}};
        txn.logWrite(rec);
        storage["accounts"].push_back(rec.newRow);

        txnMgr.commit(txn.id());
        std::cout << "COMMIT txn " << txn.id() << "\n";
        std::cout << "Rows after commit: " << storage["accounts"].size() << "\n\n";
    }

    std::cout << "=== Test 2: ROLLBACK undoes insert ===\n";
    {
        auto& txn = txnMgr.begin();
        std::cout << "BEGIN txn " << txn.id() << "\n";

        mydb::WriteRecord rec;
        rec.type   = mydb::WriteRecord::Type::Insert;
        rec.table  = "accounts";
        rec.key    = "99";
        rec.newRow = {{"id","99"}, {"name","Temp"}, {"balance","0"}};
        txn.logWrite(rec);
        storage["accounts"].push_back(rec.newRow);

        std::cout << "Rows before rollback: " << storage["accounts"].size() << "\n";
        txnMgr.rollback(txn.id(), storage);
        std::cout << "ROLLBACK txn " << txn.id() << "\n";
        std::cout << "Rows after rollback: " << storage["accounts"].size() << "\n\n";
    }

    std::cout << "=== Test 3: Lock Manager ===\n";
    {
        mydb::LockManager lm;
        bool ok1 = lm.acquireLock(1, "accounts:1", mydb::LockMode::Shared);
        bool ok2 = lm.acquireLock(2, "accounts:1", mydb::LockMode::Shared);
        bool ok3 = lm.acquireLock(3, "accounts:1", mydb::LockMode::Exclusive);

        std::cout << "Txn1 shared lock:    " << (ok1 ? "granted" : "denied") << "\n";
        std::cout << "Txn2 shared lock:    " << (ok2 ? "granted" : "denied") << "\n";
        std::cout << "Txn3 exclusive lock: " << (ok3 ? "granted" : "denied") << "\n";

        lm.releaseAll(1);
        lm.releaseAll(2);
        bool ok4 = lm.acquireLock(3, "accounts:1", mydb::LockMode::Exclusive);
        std::cout << "Txn3 exclusive (after release): " << (ok4 ? "granted" : "denied") << "\n";
    }

    return 0;
}