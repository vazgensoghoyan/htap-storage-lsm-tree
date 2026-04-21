#include <stdexcept>

#include "executor/executor.hpp"

namespace htap::executor {

Executor::Executor(storage::IStorageEngine& storage_engine) 
    : storage_engine_(storage_engine) {
}

ExecutionResult Executor::Execute(const BoundStatement& statement) {
    if (const auto* create_table = dynamic_cast<const BoundCreateTableStatement*>(&statement)) {
        return ExecuteCreateTable(*create_table);
    }

    if (const auto* insert = dynamic_cast<const BoundInsertStatement*>(&statement)) {
        return ExecuteInsert(*insert);
    }

    if (const auto* select = dynamic_cast<const BoundSelectStatement*>(&statement)) {
        return ExecuteSelect(*select);
    }

    throw std::runtime_error("Unsupported bound statement type");
}

CreateTableResult Executor::ExecuteCreateTable(const BoundCreateTableStatement& statement) {
    storage_engine_.create_table(statement.table_name, statement.schema);
    return CreateTableResult{};
}

InsertResult Executor::ExecuteInsert(const BoundInsertStatement& statement) {
    storage_engine_.insert(statement.table_name, statement.row_values);
    return InsertResult{1};
}

SelectResult Executor::ExecuteSelect(const BoundSelectStatement& statement) {
    (void)statement;
    throw std::runtime_error("ExecuteSelect is not implemented yet");
}



}

