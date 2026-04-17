#pragma once

#include <memory>

#include "parser/ast.hpp"
#include "storage/api/storage_engine_interface.hpp"
#include "executor/bound_statement.hpp"

namespace htap::executor {

class Binder {
public:
    explicit Binder(const storage::IStorageEngine& storage_engine);

    std::unique_ptr<BoundStatement> Bind(const parser::Statement& statement) const;

private:
    std::unique_ptr<BoundCreateTableStatement> BindCreateTable(const parser::CreateTableStatement& statement) const;
    std::unique_ptr<BoundInsertStatement> BindInsert(const parser::InsertStatement& statement) const;
    std::unique_ptr<BoundSelectStatement> BindSelect(const parser::SelectStatement& statement) const;

private:
    const storage::IStorageEngine& storage_engine_;
};

}