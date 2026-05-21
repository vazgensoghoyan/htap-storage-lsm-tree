#include <stdexcept>
#include <limits>
#include <algorithm>

#include "executor/executor.hpp"

namespace htap::executor {


namespace {

struct GlobalAggregateState {
        std::int64_t count = 0;

        std::int64_t int_sum = 0;
        std::int64_t int_min = std::numeric_limits<std::int64_t>::max();
        std::int64_t int_max = std::numeric_limits<std::int64_t>::min();

        double double_sum = 0;
        double double_min = std::numeric_limits<double>::max();
        double double_max = std::numeric_limits<double>::lowest();
};

void UpdateGlobalAggregateState(
    const NullableResultValue& value, 
    const BoundSelectAggregateExpression& aggregate, 
    GlobalAggregateState& aggregate_state
) {
    if (!value.has_value()) {
        return;
    }

    switch (aggregate.kind) {
        case parser::AggregateKind::Count:
            ++aggregate_state.count;
            return;

        case parser::AggregateKind::Avg:
            ++aggregate_state.count;

            if (const auto* double_value = std::get_if<double>(&*value)) {
                aggregate_state.double_sum += *double_value;
                return;
            }

            if (const auto* int_value = std::get_if<std::int64_t>(&*value)) {
                aggregate_state.double_sum += static_cast<double>(*int_value);
                return;
            }

            throw std::runtime_error("AVG expects numeric value");

        case parser::AggregateKind::Sum:
            ++aggregate_state.count;

            if (aggregate.result_type == ExpressionType::Int64) {
                aggregate_state.int_sum += std::get<std::int64_t>(*value);
                return;
            }

            if (aggregate.result_type == ExpressionType::Double) {
                aggregate_state.double_sum += std::get<double>(*value);
                return;
            }

            throw std::runtime_error("Unsupported SUM result type");

        case parser::AggregateKind::Min:
            ++aggregate_state.count;

            if (aggregate.result_type == ExpressionType::Int64) {
                std::int64_t current = std::get<std::int64_t>(*value);
                if (current < aggregate_state.int_min) {
                    aggregate_state.int_min = current;
                }
                return;
            }

            if (aggregate.result_type == ExpressionType::Double) {
                double current = std::get<double>(*value);
                if (current < aggregate_state.double_min) {
                    aggregate_state.double_min = current;
                }
                return;
            }

            throw std::runtime_error("Unsupported MIN result type");

        case parser::AggregateKind::Max:
            ++aggregate_state.count;

            if (aggregate.result_type == ExpressionType::Int64) {
                std::int64_t current = std::get<std::int64_t>(*value);
                if (current > aggregate_state.int_max) {
                    aggregate_state.int_max = current;
                }
                return;
            }

            if (aggregate.result_type == ExpressionType::Double) {
                double current = std::get<double>(*value);
                if (current > aggregate_state.double_max) {
                    aggregate_state.double_max = current;
                }
                return;
            }

            throw std::runtime_error("Unsupported MAX result type");
    }

    throw std::runtime_error("Unsupported aggregate kind");
}

NullableResultValue FinalizeAggregateState(
    const BoundSelectAggregateExpression& aggregate, 
    const GlobalAggregateState& aggregate_state
) {
    switch (aggregate.kind) {
        case parser::AggregateKind::Count: 
            return aggregate_state.count;
        
        case parser::AggregateKind::Avg:
            if (aggregate_state.count == 0) return std::nullopt;

            if (aggregate.result_type == ExpressionType::Double) {
                return aggregate_state.double_sum / aggregate_state.count;
            }

            throw std::runtime_error("Unsupported AVG result type");

        case parser::AggregateKind::Sum:
            if (aggregate_state.count == 0) return std::nullopt;

            if (aggregate.result_type == ExpressionType::Int64) {
                return aggregate_state.int_sum;
            }

            if (aggregate.result_type == ExpressionType::Double) {
                return aggregate_state.double_sum;
            }

            throw std::runtime_error("Unsupported SUM result type");

        case parser::AggregateKind::Min:
            if (aggregate_state.count == 0) return std::nullopt;

            if (aggregate.result_type == ExpressionType::Int64) {
                return aggregate_state.int_min;
            }

            if (aggregate.result_type == ExpressionType::Double) {
                return aggregate_state.double_min;
            }

            throw std::runtime_error("Unsupported MIN result type");

        case parser::AggregateKind::Max:
            if (aggregate_state.count == 0) return std::nullopt;

            if (aggregate.result_type == ExpressionType::Int64) {
                return aggregate_state.int_max;
            }

            if (aggregate.result_type == ExpressionType::Double) {
                return aggregate_state.double_max;
            }

            throw std::runtime_error("Unsupported MAX result type");
    }

    throw std::runtime_error("Unsupported aggregation kind");
}

void CollectConjuncts(const BoundExpression& expression, std::vector<const BoundExpression*>& conjuncts) {
    if (expression.kind == BoundExpressionKind::Binary) {
        const auto& binary = static_cast<const BoundBinaryExpression&>(expression);

        if (binary.operation == parser::BinaryOperation::And) {
            CollectConjuncts(*binary.left, conjuncts);
            CollectConjuncts(*binary.right, conjuncts);
            return;
        }
    }

    conjuncts.push_back(&expression);
}

int CompareResultValues(const ResultValue& left, const ResultValue& right) {
    if (const auto* left_int = std::get_if<std::int64_t>(&left)) {
        if (const auto* right_int = std::get_if<std::int64_t>(&right)) {
            if (*left_int < *right_int) return -1;
            if (*left_int > *right_int) return 1;
            return 0;
        }

        if (const auto* right_double = std::get_if<double>(&right)) {
            double left_value = static_cast<double>(*left_int);
            if (left_value < *right_double) return -1;
            if (left_value > *right_double) return 1;
            return 0;
        }

        throw std::runtime_error("Incompatible types in comparison");
    }

    if (const auto* left_double = std::get_if<double>(&left)) {
        if (const auto* right_double = std::get_if<double>(&right)) {
            if (*left_double < *right_double) return -1;
            if (*left_double > *right_double) return 1;
            return 0;
        }

        if (const auto* right_int = std::get_if<std::int64_t>(&right)) {
            double right_value = static_cast<double>(*right_int);
            if (*left_double < right_value) return -1;
            if (*left_double > right_value) return 1;
            return 0;
        }

        throw std::runtime_error("Incompatible types in comparison");
    }

    if (const auto* left_string = std::get_if<std::string>(&left)) {
        const auto* right_string = std::get_if<std::string>(&right);
        if (!right_string) {
            throw std::runtime_error("Incompatible types in comparison");
        }

        if (*left_string < *right_string) return -1;
        if (*left_string > *right_string) return 1;
        return 0;
    }

    if (const auto* left_bool = std::get_if<bool>(&left)) {
        const auto* right_bool = std::get_if<bool>(&right);
        if (!right_bool) {
            throw std::runtime_error("Incompatible types in comparison");
        }

        if (*left_bool == *right_bool) return 0;
        return *left_bool ? 1 : -1;
    }

    throw std::runtime_error("Unsupported value type in comparison");
}

int CompareNullableResultValues(const NullableResultValue& left, const NullableResultValue& right) {
    if (!left.has_value() && !right.has_value()) {
        return 0;
    }

    if (!left.has_value()) {
        return 1;
    }

    if (!right.has_value()) {
        return -1;
    }

    return CompareResultValues(*left, *right);
}

}

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

SelectResult Executor::ExecutePlainSelect(
    const BoundSelectStatement& statement,
    const std::vector<std::string>& column_names,
    const std::vector<std::size_t>& projection,
    bool can_apply_early_limit
) {
    if (!statement.schema) {
        throw std::runtime_error("BoundSelectStatement schema is null");
    }

    SelectResult result;
    result.column_names = column_names;

    struct PlainSortRow {
        std::vector<NullableResultValue> sort_keys;
        ResultRow row;
    };
    std::vector<PlainSortRow> sorted_rows;

    SelectCursorBuildResult cursor_build_result = BuildCursorForSelect(statement, projection);
    if (cursor_build_result.empty_result) {
        return result;
    }

    std::unique_ptr<storage::ICursor> cursor = std::move(cursor_build_result.cursor);

    while (cursor->valid()) {

        if (statement.where_expression && !EvaluatePredicate(*statement.where_expression, *cursor)) {
            cursor->next();
            continue;
        }

        ResultRow row;
        row.reserve(statement.select_items.size());

        for (const auto& item : statement.select_items) {
            const auto* expression_item = dynamic_cast<const BoundSelectItemExpression*>(item.get());

            if (!expression_item) {
                throw std::runtime_error("Plain SELECT does not support aggregate select items");
            }

            row.push_back(EvaluateExpression(*expression_item->expression, *cursor));

        }

        if (statement.order_by_items.empty()) {
            result.rows.push_back(std::move(row));

            if (can_apply_early_limit && statement.limit.has_value() &&
                result.rows.size() >= *statement.limit) {
                break;
            }
        } else {
            std::vector<NullableResultValue> sort_keys;
            sort_keys.reserve(statement.order_by_items.size());

            for (const auto& item : statement.order_by_items) {
                sort_keys.push_back(EvaluateExpression(*item.expression, *cursor));
            }

            sorted_rows.push_back(
                PlainSortRow {
                    std::move(sort_keys),
                    std::move(row)
                }
            );
        }

        cursor->next();
    }

    if (!statement.order_by_items.empty()) {
        std::sort(
            sorted_rows.begin(),
            sorted_rows.end(),
            [&](const PlainSortRow& x, const PlainSortRow& y) {
                for (std::size_t i = 0; i < statement.order_by_items.size(); ++i) {
                    int cmp = CompareNullableResultValues(x.sort_keys[i], y.sort_keys[i]);

                    if (cmp == 0) continue;

                    if (statement.order_by_items[i].direction == parser::OrderDirection::Asc) {
                        return (cmp < 0);
                    }

                    if (statement.order_by_items[i].direction == parser::OrderDirection::Desc) {
                        return (cmp > 0);
                    }

                    throw std::runtime_error("Unsupported order direction");
                }

                return false;
            }
        );


        result.rows.reserve(sorted_rows.size());

        for (auto& sorted_row : sorted_rows) {
            result.rows.push_back(std::move(sorted_row.row));
        }

    }

    if (!can_apply_early_limit && statement.limit.has_value()) {
        if (result.rows.size() > *statement.limit) {
            result.rows.resize(*statement.limit);
        }
    }

    return result;
}

Executor::SelectCursorBuildResult Executor::BuildCursorForSelect(
    const BoundSelectStatement& statement,
    const std::vector<std::size_t>& projection
) const {
    SelectCursorBuildResult build_result;

    std::vector<const BoundExpression*> conjuncts;
    if (statement.where_expression) {
        CollectConjuncts(*statement.where_expression, conjuncts);
    }

    std::optional<storage::Key> point_key;
    bool found_point_lookup = false;

    std::optional<storage::Key> from;
    std::optional<storage::Key> to;
    bool found_any_range_predicate = false;

    bool empty_result = false;

    for (const BoundExpression* conjunct : conjuncts) {
        if (conjunct->kind != BoundExpressionKind::Binary) {
            continue;
        }

        const auto& binary = static_cast<const BoundBinaryExpression&>(*conjunct);

        const BoundExpression* left = binary.left.get();
        const BoundExpression* right = binary.right.get();

        const BoundColumnExpression* key_column = nullptr;
        const BoundLiteralExpression* key_literal = nullptr;
        bool column_on_left = false;

        if (left->kind == BoundExpressionKind::Column && right->kind == BoundExpressionKind::Literal) {
            key_column = static_cast<const BoundColumnExpression*>(left);
            key_literal = static_cast<const BoundLiteralExpression*>(right);
            column_on_left = true;
        } else if (left->kind == BoundExpressionKind::Literal && right->kind == BoundExpressionKind::Column) {
            key_column = static_cast<const BoundColumnExpression*>(right);
            key_literal = static_cast<const BoundLiteralExpression*>(left);
            column_on_left = false;
        } else {
            continue;
        }

        if (!statement.schema->get_column(key_column->column_index).is_key) {
            continue;
        }

        if (!key_literal->value.has_value()) {
            empty_result = true;
            break;
        }

        storage::Key key = std::get<std::int64_t>(*key_literal->value);

        switch (binary.operation) {
            case parser::BinaryOperation::Equal:
                if (!found_point_lookup) {
                    point_key = key;
                    found_point_lookup = true;
                } else if (*point_key != key) {
                    empty_result = true;
                }
                break;

            case parser::BinaryOperation::Greater:
                found_any_range_predicate = true;
                if (column_on_left) {
                    if (key == std::numeric_limits<std::int64_t>::max()) {
                        empty_result = true;
                    } else if (!from.has_value() || key + 1 > *from) {
                        from = key + 1;
                    }
                } else {
                    if (!to.has_value() || key < *to) {
                        to = key;
                    }
                }
                break;

            case parser::BinaryOperation::GreaterEqual:
                found_any_range_predicate = true;
                if (column_on_left) {
                    if (!from.has_value() || key > *from) {
                        from = key;
                    }
                } else {
                    if (key != std::numeric_limits<std::int64_t>::max()) {
                        if (!to.has_value() || key + 1 < *to) {
                            to = key + 1;
                        }
                    }
                }
                break;

            case parser::BinaryOperation::Less:
                found_any_range_predicate = true;
                if (column_on_left) {
                    if (!to.has_value() || key < *to) {
                        to = key;
                    }
                } else {
                    if (key == std::numeric_limits<std::int64_t>::max()) {
                        empty_result = true;
                    } else if (!from.has_value() || key + 1 > *from) {
                        from = key + 1;
                    }
                }
                break;

            case parser::BinaryOperation::LessEqual:
                found_any_range_predicate = true;
                if (column_on_left) {
                    if (key != std::numeric_limits<std::int64_t>::max()) {
                        if (!to.has_value() || key + 1 < *to) {
                            to = key + 1;
                        }
                    }
                } else {
                    if (!from.has_value() || key > *from) {
                        from = key;
                    }
                }
                break;

            default:
                break;
        }

        if (empty_result) {
            break;
        }
    }

    if (empty_result) {
        build_result.empty_result = true;
        return build_result;
    }

    if (found_point_lookup) {
        build_result.cursor = storage_engine_.get(
            statement.table_name,
            *point_key,
            projection
        );
        return build_result;
    }

    if (found_any_range_predicate) {
        if (from.has_value() && to.has_value() && *from >= *to) {
            build_result.empty_result = true;
            return build_result;
        }

        build_result.cursor = storage_engine_.scan(
            statement.table_name,
            from,
            to,
            projection
        );
        return build_result;
    }

    build_result.cursor = storage_engine_.scan(
        statement.table_name,
        std::nullopt,
        std::nullopt,
        projection
    );
    return build_result;
}

bool Executor::EvaluatePredicate(const BoundExpression& expression, const storage::ICursor& cursor) const {
    NullableResultValue value = EvaluateExpression(expression, cursor);

    if (!value.has_value()) return false;

    if (const auto* boolean_value = std::get_if<bool>(&*value)) {
        return *boolean_value;
    }

    throw std::runtime_error("WHERE expression must evaluate to BOOLEAN");
}

NullableResultValue Executor::EvaluateExpression(
    const BoundExpression& expression,
    const storage::ICursor& cursor
) const {
    switch (expression.kind) {
        case BoundExpressionKind::Literal: {
            const auto& literal = static_cast<const BoundLiteralExpression&>(expression);

            if (!literal.value.has_value()) {
                return std::nullopt;
            }

            return std::visit(
                [](const auto& value) -> ResultValue {
                    return value;
                },
                *literal.value
            );
        }

        case BoundExpressionKind::Column: {
            const auto& column = static_cast<const BoundColumnExpression&>(expression);
            storage::NullableValue value = cursor.value(column.column_index);

            if (!value.has_value()) {
                return std::nullopt;
            }

            return std::visit(
                [](const auto& inner_value) -> ResultValue {
                    return inner_value;
                },
                *value
            );
        }

        case BoundExpressionKind::Unary: {
            const auto& unary = static_cast<const BoundUnaryExpression&>(expression);

            NullableResultValue value = EvaluateExpression(*unary.expression, cursor);

            if (!value.has_value()) {
                return std::nullopt;
            }

            const auto* boolean_value = std::get_if<bool>(&*value);
            if (!boolean_value) {
                throw std::runtime_error("Unary NOT expects BOOLEAN");
            }

            switch (unary.operation) {
                case parser::UnaryOperation::Not:
                    return !(*boolean_value);
            }

            throw std::runtime_error("Unsupported unary operation");
        }

        case BoundExpressionKind::Binary: {
            const auto& binary = static_cast<const BoundBinaryExpression&>(expression);

            if (binary.operation == parser::BinaryOperation::And) {
                NullableResultValue left = EvaluateExpression(*binary.left, cursor);
                NullableResultValue right = EvaluateExpression(*binary.right, cursor);

                if (left.has_value()) {
                    const auto* left_bool = std::get_if<bool>(&*left);
                    if (!left_bool) {
                        throw std::runtime_error("AND expects BOOLEAN operands");
                    }

                    if (!*left_bool) {
                        return false;
                    }
                }

                if (right.has_value()) {
                    const auto* right_bool = std::get_if<bool>(&*right);
                    if (!right_bool) {
                        throw std::runtime_error("AND expects BOOLEAN operands");
                    }

                    if (!*right_bool) {
                        return false;
                    }
                }

                if (!left.has_value() || !right.has_value()) {
                    return std::nullopt;
                }

                return true;
            }

            if (binary.operation == parser::BinaryOperation::Or) {
                NullableResultValue left = EvaluateExpression(*binary.left, cursor);
                NullableResultValue right = EvaluateExpression(*binary.right, cursor);

                if (left.has_value()) {
                    const auto* left_bool = std::get_if<bool>(&*left);
                    if (!left_bool) {
                        throw std::runtime_error("OR expects BOOLEAN operands");
                    }

                    if (*left_bool) {
                        return true;
                    }
                }

                if (right.has_value()) {
                    const auto* right_bool = std::get_if<bool>(&*right);
                    if (!right_bool) {
                        throw std::runtime_error("OR expects BOOLEAN operands");
                    }

                    if (*right_bool) {
                        return true;
                    }
                }

                if (!left.has_value() || !right.has_value()) {
                    return std::nullopt;
                }

                return false;
            }

            NullableResultValue left = EvaluateExpression(*binary.left, cursor);
            NullableResultValue right = EvaluateExpression(*binary.right, cursor);

            if (!left.has_value() || !right.has_value()) {
                return std::nullopt;
            }

            int comparison = CompareResultValues(*left, *right);

            switch (binary.operation) {
                case parser::BinaryOperation::Equal:
                    return comparison == 0;

                case parser::BinaryOperation::NotEqual:
                    return comparison != 0;

                case parser::BinaryOperation::Less:
                    return comparison < 0;

                case parser::BinaryOperation::LessEqual:
                    return comparison <= 0;

                case parser::BinaryOperation::Greater:
                    return comparison > 0;

                case parser::BinaryOperation::GreaterEqual:
                    return comparison >= 0;

                default:
                    throw std::runtime_error("Unsupported binary operation");
            }
        }

        case BoundExpressionKind::IsNull: {
            const auto& is_null = static_cast<const BoundIsNullExpression&>(expression);

            NullableResultValue value = EvaluateExpression(*is_null.expression, cursor);

            bool result = !value.has_value();

            if (is_null.is_not) {
                result = !result;
            }

            return result;
        }
    }

    throw std::runtime_error("Unsupported bound expression kind");
}

SelectResult Executor::ExecuteGlobalAggregateSelect(
    const BoundSelectStatement& statement,
    const std::vector<std::string>& column_names,
    const std::vector<std::size_t>& projection
) {
    if (!statement.schema) {
        throw std::runtime_error("BoundSelectStatement schema is null");
    }

    SelectResult result;
    result.column_names = column_names;

    std::vector<const BoundSelectAggregateExpression*> aggregates;
    aggregates.reserve(statement.select_items.size());

    std::vector<GlobalAggregateState> aggregate_states;
    aggregate_states.reserve(statement.select_items.size());

    for (const auto& item : statement.select_items) {
        const auto* aggregate = dynamic_cast<const BoundSelectAggregateExpression*>(item.get());

        if (!aggregate) throw std::runtime_error("Global aggregate SELECT expects aggregate select items only");

        aggregates.push_back(aggregate);
        aggregate_states.push_back(GlobalAggregateState{});
    }

    SelectCursorBuildResult cursor_build_result = BuildCursorForSelect(statement, projection);


    if (!cursor_build_result.empty_result) {
        std::unique_ptr<storage::ICursor> cursor = std::move(cursor_build_result.cursor);

        while (cursor->valid()) {
            if (statement.where_expression && !EvaluatePredicate(*statement.where_expression, *cursor)) {
                cursor->next();
                continue;
            }

            for (std::size_t i = 0; i < aggregates.size(); ++i) {
                NullableResultValue value = EvaluateExpression(*aggregates[i]->expression, *cursor);
                UpdateGlobalAggregateState(value, *aggregates[i], aggregate_states[i]);
            }

            cursor->next();
        }
    }

    ResultRow row;
    row.reserve(aggregates.size());

    for (std::size_t i = 0; i < aggregates.size(); ++i) {
        row.push_back(
            FinalizeAggregateState(*aggregates[i], aggregate_states[i])
        );
    }

    if (!statement.limit.has_value() || *statement.limit > 0) {
        result.rows.push_back(std::move(row));
    }

    return result;
}

SelectResult Executor::ExecuteGroupedAggregateSelect(
    const BoundSelectStatement& statement,
    const std::vector<std::string>& column_names,
    const std::vector<std::size_t>& projection
) {
    (void)statement;
    (void)column_names;
    (void)projection;
    throw std::runtime_error("ExecuteGroupedAggregateSelect is not implemented yet");
}

}
