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
    const std::vector<std::string> column_names = BuildSelectColumnNames(statement);
    const std::vector<std::size_t> projection = CollectRequiredProjection(statement);
    
    const bool has_aggregates = HasAggregateSelectItems(statement);
    const bool has_group_by = !statement.group_by_items.empty();
    const bool can_apply_early_limit = CanApplyEarlyLimit(statement);

    if (!has_aggregates && !has_group_by) {
        return ExecutePlainSelect(
            statement,
            column_names,
            projection,
            can_apply_early_limit
        );
    }

    if (has_aggregates && !has_group_by) {
        return ExecuteGlobalAggregateSelect(
            statement,
            column_names,
            projection
        );
    }

    return ExecuteGroupedAggregateSelect(
        statement,
        column_names,
        projection
    );

}

std::vector<std::string> Executor::BuildSelectColumnNames(const BoundSelectStatement& statement) const {
    if (!statement.schema) {
        throw std::runtime_error("BoundSelectStatement schema is null");
    }

    std::vector<std::string> column_names;
    column_names.reserve(statement.select_items.size());

    for (std::size_t i = 0; i < statement.select_items.size(); ++i) {
        const BoundSelectItem* item = statement.select_items[i].get();

        if (const auto* expression_item = dynamic_cast<const BoundSelectItemExpression*>(item)) {
            if (const auto column_item = 
                    dynamic_cast<const BoundColumnExpression*>(expression_item->expression.get())) {
                    
                column_names.push_back(statement.schema->get_column(column_item->column_index).name);
            } else {
                column_names.push_back("expression" + std::to_string(i + 1));
            }

            continue;
        }
        
        if (const auto* aggregate_item = dynamic_cast<const BoundSelectAggregateExpression*>(item)) {
            std::string aggregate_name;

            switch (aggregate_item->kind) {
                case parser::AggregateKind::Count:
                    aggregate_name = "count";
                    break;
                case parser::AggregateKind::Sum:
                    aggregate_name = "sum";
                    break;
                case parser::AggregateKind::Avg:
                    aggregate_name = "avg";
                    break;
                case parser::AggregateKind::Min:
                    aggregate_name = "min";
                    break;
                case parser::AggregateKind::Max:
                    aggregate_name = "max";
                    break;
            }

            if (const auto* column_expression =
                    dynamic_cast<const BoundColumnExpression*>(aggregate_item->expression.get())) {

                column_names.push_back(
                    aggregate_name + "(" + statement.schema->get_column(column_expression->column_index).name + ")"
                );
            } else {
                column_names.push_back(
                    aggregate_name + "(expression" + std::to_string(i + 1) + ")"
                );
            }

            continue;
        } 
        
        throw std::runtime_error("Unsupported bound select item");
    }

    return column_names;
}

std::vector<std::size_t> Executor::CollectRequiredProjection(const BoundSelectStatement& statement) const {
    if (!statement.schema) {
        throw std::runtime_error("BoundSelectStatement schema is null");
    }

    std::vector<std::size_t> projection;
    std::vector<bool> used_columns(statement.schema->size(), false);

    for (const auto& select_item : statement.select_items) {
        const BoundSelectItem* item = select_item.get();

        if (const auto* expression_item = dynamic_cast<const BoundSelectItemExpression*>(item)) {
            CollectProjectionFromExpression(
                *expression_item->expression,
                projection,
                used_columns
            );
            continue;
        }

        if (const auto* aggregate_item = dynamic_cast<const BoundSelectAggregateExpression*>(item)) {
            CollectProjectionFromExpression(
                *aggregate_item->expression,
                projection,
                used_columns
            );
            continue;
        }

        throw std::runtime_error("Unsupported bound select item");
    }

    if (statement.where_expression) {
        CollectProjectionFromExpression(
            *statement.where_expression,
            projection,
            used_columns
        );
    }

    for (const auto& group_by_item : statement.group_by_items) {
        CollectProjectionFromExpression(
            *group_by_item,
            projection,
            used_columns
        );
    }

    for (const auto& order_by_item : statement.order_by_items) {
        CollectProjectionFromExpression(
            *order_by_item.expression,
            projection,
            used_columns
        );
    }

    return projection;

}

void Executor::CollectProjectionFromExpression(
        const BoundExpression& expression, 
        std::vector<std::size_t>& projection, 
        std::vector<bool>& used_columns
    ) const {
    
    switch (expression.kind) {
        case BoundExpressionKind::Literal:
            return;

        case BoundExpressionKind::Column: {
            const auto& column = static_cast<const BoundColumnExpression&>(expression);
            if (!used_columns[column.column_index]) {
                used_columns[column.column_index] = true;
                projection.push_back(column.column_index);
            }
            return;
        }

        case BoundExpressionKind::Unary: {
            const auto& unary = static_cast<const BoundUnaryExpression&>(expression);
            CollectProjectionFromExpression(
                *unary.expression,
                projection,
                used_columns
            );
            return;
        }

        case BoundExpressionKind::Binary: {
            const auto& binary = static_cast<const BoundBinaryExpression&>(expression);
            CollectProjectionFromExpression(
                *binary.left,
                projection,
                used_columns
            );
            CollectProjectionFromExpression(
                *binary.right,
                projection,
                used_columns
            );
            return;
        }

        case BoundExpressionKind::IsNull: {
            const auto& is_null = static_cast<const BoundIsNullExpression&>(expression);
            CollectProjectionFromExpression(
                *is_null.expression,
                projection,
                used_columns
            );
            return;
        }

    }
    throw std::runtime_error("Unsupported bound expression kind");

}

bool Executor::HasAggregateSelectItems(const BoundSelectStatement& statement) const {
    for (const auto& item : statement.select_items) {
        if (dynamic_cast<const BoundSelectAggregateExpression*>(item.get())) {
            return true;
        }
    }

    return false;
}

bool Executor::CanApplyEarlyLimit(const BoundSelectStatement& statement) const {
    if (!statement.limit.has_value()) return false;

    if (!statement.order_by_items.empty()) return false;

    if (!statement.group_by_items.empty()) return false;

    if (HasAggregateSelectItems(statement)) return false;

    return true;
}

}

