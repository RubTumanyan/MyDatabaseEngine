#include <gtest/gtest.h>
#include "txn_manager.h"

using namespace mydb;

// ── Helper: создаём тестовую таблицу ─────────────────────────────
static TableStorage makeStorage() {
    TableStorage s;
    s["users"] = {
        {{"id","1"}, {"name","Alice"}, {"age","30"}},
        {{"id","2"}, {"name","Bob"},   {"age","25"}},
        {{"id","3"}, {"name","Carol"}, {"age","35"}},
    };
    return s;
}

// ─────────────────────────────────────────────────────────────────
//  Transaction basic tests
// ─────────────────────────────────────────────────────────────────

TEST(TransactionTest, BeginCreatesActiveTransaction) {
    TransactionManager mgr;
    auto& txn = mgr.begin();

    EXPECT_EQ(txn.status(), TxnStatus::Active);
    EXPECT_TRUE(txn.active());
    EXPECT_EQ(txn.id(), 1u);
}

TEST(TransactionTest, EachTransactionGetsUniqueId) {
    TransactionManager mgr;
    auto& t1 = mgr.begin();
    auto& t2 = mgr.begin();
    auto& t3 = mgr.begin();

    EXPECT_NE(t1.id(), t2.id());
    EXPECT_NE(t2.id(), t3.id());
}

TEST(TransactionTest, CommitChangesStatus) {
    TransactionManager mgr;
    auto& txn = mgr.begin();
    TxnId id  = txn.id();

    mgr.commit(id);

    EXPECT_EQ(mgr.get(id).status(), TxnStatus::Committed);
    EXPECT_FALSE(mgr.get(id).active());
}

TEST(TransactionTest, RollbackChangesStatus) {
    TransactionManager mgr;
    auto storage = makeStorage();
    auto& txn = mgr.begin();
    TxnId id  = txn.id();

    mgr.rollback(id, storage);

    EXPECT_EQ(mgr.get(id).status(), TxnStatus::Aborted);
    EXPECT_FALSE(mgr.get(id).active());
}

TEST(TransactionTest, CommitOnNonActiveTxnThrows) {
    TransactionManager mgr;
    auto& txn = mgr.begin();
    TxnId id  = txn.id();

    mgr.commit(id);
    EXPECT_THROW(mgr.commit(id), std::runtime_error);
}

// ─────────────────────────────────────────────────────────────────
//  Write log tests
// ─────────────────────────────────────────────────────────────────

TEST(TransactionTest, WriteLogRecordsInsert) {
    TransactionManager mgr;
    auto& txn = mgr.begin();

    WriteRecord rec;
    rec.type   = WriteRecord::Type::Insert;
    rec.table  = "users";
    rec.key    = "4";
    rec.newRow = {{"id","4"}, {"name","Dave"}};
    txn.logWrite(rec);

    EXPECT_EQ(txn.writeLog().size(), 1u);
    EXPECT_EQ(txn.writeLog()[0].key, "4");
    EXPECT_EQ(txn.writeLog()[0].type, WriteRecord::Type::Insert);
}

TEST(TransactionTest, WriteLogCanHoldMultipleRecords) {
    TransactionManager mgr;
    auto& txn = mgr.begin();

    for (int i = 0; i < 5; ++i) {
        WriteRecord rec;
        rec.type  = WriteRecord::Type::Insert;
        rec.table = "users";
        rec.key   = std::to_string(i);
        txn.logWrite(rec);
    }

    EXPECT_EQ(txn.writeLog().size(), 5u);
}

// ─────────────────────────────────────────────────────────────────
//  Rollback correctness tests
// ─────────────────────────────────────────────────────────────────

TEST(TransactionTest, RollbackUndoesInsert) {
    TransactionManager mgr;
    auto storage = makeStorage();
    auto& txn    = mgr.begin();

    // Insert a new row
    WriteRecord rec;
    rec.type   = WriteRecord::Type::Insert;
    rec.table  = "users";
    rec.key    = "99";
    rec.newRow = {{"id","99"}, {"name","Temp"}};
    txn.logWrite(rec);
    storage["users"].push_back(rec.newRow);

    EXPECT_EQ(storage["users"].size(), 4u); // 3 + 1 inserted

    mgr.rollback(txn.id(), storage);

    EXPECT_EQ(storage["users"].size(), 3u); // back to original
}

TEST(TransactionTest, RollbackUndoesUpdate) {
    TransactionManager mgr;
    auto storage = makeStorage();
    auto& txn    = mgr.begin();

    // Simulate update: change Alice's age from 30 to 99
    WriteRecord rec;
    rec.type   = WriteRecord::Type::Update;
    rec.table  = "users";
    rec.key    = "1";
    rec.oldRow = {{"id","1"}, {"name","Alice"}, {"age","30"}};
    rec.newRow = {{"id","1"}, {"name","Alice"}, {"age","99"}};
    txn.logWrite(rec);

    // Apply the update
    for (auto& row : storage["users"]) {
        if (row["id"] == "1") { row["age"] = "99"; break; }
    }

    // Verify update applied
    EXPECT_EQ(storage["users"][0]["age"], "99");

    mgr.rollback(txn.id(), storage);

    // Verify rollback restored old value
    EXPECT_EQ(storage["users"][0]["age"], "30");
}

TEST(TransactionTest, CommitDoesNotModifyStorage) {
    TransactionManager mgr;
    auto storage = makeStorage();
    auto& txn    = mgr.begin();

    // Insert and commit
    WriteRecord rec;
    rec.type   = WriteRecord::Type::Insert;
    rec.table  = "users";
    rec.key    = "4";
    rec.newRow = {{"id","4"}, {"name","Dave"}};
    txn.logWrite(rec);
    storage["users"].push_back(rec.newRow);

    mgr.commit(txn.id());

    // After commit, row should still be there
    EXPECT_EQ(storage["users"].size(), 4u);
}

// ─────────────────────────────────────────────────────────────────
//  Lock Manager tests
// ─────────────────────────────────────────────────────────────────

TEST(LockManagerTest, SharedLocksAreCompatible) {
    LockManager lm;
    EXPECT_TRUE(lm.acquireLock(1, "users:1", LockMode::Shared));
    EXPECT_TRUE(lm.acquireLock(2, "users:1", LockMode::Shared));
    EXPECT_TRUE(lm.acquireLock(3, "users:1", LockMode::Shared));
}

TEST(LockManagerTest, ExclusiveBlocksShared) {
    LockManager lm;
    EXPECT_TRUE(lm.acquireLock(1, "users:1", LockMode::Exclusive));
    EXPECT_FALSE(lm.acquireLock(2, "users:1", LockMode::Shared));
}

TEST(LockManagerTest, SharedBlocksExclusive) {
    LockManager lm;
    EXPECT_TRUE(lm.acquireLock(1, "users:1", LockMode::Shared));
    EXPECT_FALSE(lm.acquireLock(2, "users:1", LockMode::Exclusive));
}

TEST(LockManagerTest, ExclusiveBlocksExclusive) {
    LockManager lm;
    EXPECT_TRUE(lm.acquireLock(1, "users:1", LockMode::Exclusive));
    EXPECT_FALSE(lm.acquireLock(2, "users:1", LockMode::Exclusive));
}

TEST(LockManagerTest, SameTransactionCanReacquire) {
    LockManager lm;
    EXPECT_TRUE(lm.acquireLock(1, "users:1", LockMode::Shared));
    // same transaction acquiring again should not conflict with itself
    EXPECT_TRUE(lm.acquireLock(1, "users:1", LockMode::Shared));
}

TEST(LockManagerTest, ReleaseAllFreesLocks) {
    LockManager lm;
    lm.acquireLock(1, "users:1", LockMode::Exclusive);
    EXPECT_FALSE(lm.acquireLock(2, "users:1", LockMode::Shared));

    lm.releaseAll(1);

    // after release txn2 should get the lock
    EXPECT_TRUE(lm.acquireLock(2, "users:1", LockMode::Shared));
}

TEST(LockManagerTest, ReleaseAllOnlyFreesOwnLocks) {
    LockManager lm;
    lm.acquireLock(1, "users:1", LockMode::Shared);
    lm.acquireLock(2, "users:1", LockMode::Shared);

    lm.releaseAll(1); // release only txn1

    // txn2 still holds shared lock — exclusive should still be denied
    EXPECT_FALSE(lm.acquireLock(3, "users:1", LockMode::Exclusive));
}

TEST(LockManagerTest, DifferentResourcesDontConflict) {
    LockManager lm;
    EXPECT_TRUE(lm.acquireLock(1, "users:1", LockMode::Exclusive));
    EXPECT_TRUE(lm.acquireLock(2, "users:2", LockMode::Exclusive));
    EXPECT_TRUE(lm.acquireLock(3, "users:3", LockMode::Exclusive));
}