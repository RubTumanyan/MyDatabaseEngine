#pragam once
#include "transaction.h"
#include <vector>

namespace mydb {

    // A versioned row — stores multiple versions for MVCC
    // Each version is visible only to transactions that started after it was written
    struct VersionedRow {
        Row      data;
        TxnId    createdBy;   // which transaction created this version
        TxnId    deletedBy;   // 0 = not deleted
    };

    // MVCC snapshot — each transaction sees the database at the moment it started.
    // Readers never block writers, writers never block readers.
    class MvccSnapshot {
    public:
        explicit MvccSnapshot(TxnId snapshotTxnId)
            : snapshotId_(snapshotTxnId) {}

        // A version is visible if it was created before this snapshot
        // and not yet deleted before this snapshot
        bool isVisible(const VersionedRow& version) const {
            bool created = version.createdBy <= snapshotId_;
            bool notDeleted = (version.deletedBy == 0)
                              || (version.deletedBy > snapshotId_);
            return created && notDeleted;
        }

        TxnId snapshotId() const { return snapshotId_; }

    private:
        TxnId snapshotId_;
    };

} // namespace mydb