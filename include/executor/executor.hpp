#pragma once

#include <cstddef>
#include <string>
#include <vector>

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

    SelectResult ExecutePlainSelect(
        const BoundSelectStatement& statement,
        const std::vector<std::string>& column_names,
        const std::vector<std::size_t>& projection,
        bool can_apply_early_limit
    );
    SelectResult ExecuteGlobalAggregateSelect(
        const BoundSelectStatement& statement,
        const std::vector<std::string>& column_names,
        const std::vector<std::size_t>& projection
    );
    SelectResult ExecuteGroupedAggregateSelect(
        const BoundSelectStatement& statement,
        const std::vector<std::string>& column_names,
        const std::vector<std::size_t>& projection
    );

    std::vector<std::string> BuildSelectColumnNames(const BoundSelectStatement& statement) const;
    std::vector<std::size_t> CollectRequiredProjection(const BoundSelectStatement& statement) const;
    void CollectProjectionFromExpression(
        const BoundExpression& expression, 
        std::vector<std::size_t>& projection, 
        std::vector<bool>& used_columns
    ) const;
    bool HasAggregateSelectItems(const BoundSelectStatement& statement) const;
    bool CanApplyEarlyLimit(const BoundSelectStatement& statement) const;

private:
    storage::IStorageEngine& storage_engine_;
};

}
