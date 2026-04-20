#pragma once

#include "storage/api/storage_engine_interface.hpp"
#include "executor/execution_result.hpp"
#include "executor/bound_statement.hpp"

namespace htap::executor {

class Executor {
public:
    explicit Executor(storage::IStorageEngine& storage_engine);
    ExecutionResult Execute(const BoundStatement& statement);

private:
    CreateTableResult ExecuteCreateTable(const BoundCreateTableStatement& statement);
    InsertResult ExecuteInsert(const BoundInsertStatement& statement);
    SelectResult ExecuteSelect(const BoundSelectStatement& statement);

private:
    storage::IStorageEngine& storage_engine_;
};

}
