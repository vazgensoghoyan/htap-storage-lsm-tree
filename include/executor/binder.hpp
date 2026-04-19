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

    storage::ValueType ParserDataTypeToStorageValueType(parser::DataType type) const;
    const storage::Schema& GetSchema(const std::string& table_name) const;
    storage::NullableValue ConvertLiteralExpression(const parser::Expression& expression) const;
    std::unique_ptr<BoundExpression> Binder::BindExpression(const parser::Expression& expression,
        const storage::Schema& schema) const;
    std::vector<std::unique_ptr<BoundSelectItem>> BindSelectItem(const parser::SelectItem& item,
        const storage::Schema& schema) const;
    ExpressionType GetAggregateResultType(parser::AggregateKind kind, ExpressionType argument_type) const;
    BoundOrderByItem Binder::BindOrderByItem(const parser::OrderByItem& item,const storage::Schema& schema) const;

    bool HasAggregates(const BoundSelectStatement& statement) const;
    void ValidateWhere(const BoundSelectStatement& statement) const;
    bool AreEquivalentExpressions(const BoundExpression& left, const BoundExpression& right) const;
    void ValidateGroupedSelect(const BoundSelectStatement& statement) const;

private:
    const storage::IStorageEngine& storage_engine_;
};

}