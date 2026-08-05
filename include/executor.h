#pragma once
#include "operator.h"
#include "physical_plan.h"
#include "mvcc.h"
#include "wal.h"
#include "ast.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace mydb {

    using TableStorage = std::unordered_map<std::string, std::vector<Row>>;

    class Transaction;

    class Executor {
    public:
        explicit Executor(TableStorage& storage) : storage_(storage) {}
        Executor(TableStorage& storage, WAL* wal)
            : storage_(storage), wal_(wal) {}

        std::vector<Row> execute(const PhysicalPlanRef& plan);

        void executeInsert(const InsertStatement& stmt,
                           Transaction* txn = nullptr);
        void executeUpdate(const UpdateStatement& stmt,
                           Transaction* txn = nullptr);
        void executeDelete(const DeleteStatement& stmt,
                           Transaction* txn = nullptr);
        void executeCreateTable(const CreateTableStatement& stmt);

        void setSnapshot(std::shared_ptr<MvccSnapshot> snap) {
            snapshot_ = std::move(snap);
        }

    private:
        TableStorage&                storage_;
        WAL*                         wal_ = nullptr;
        std::shared_ptr<MvccSnapshot> snapshot_;

        OperatorRef build(const PhysicalPlanRef& node);
    };

} // namespace mydb
