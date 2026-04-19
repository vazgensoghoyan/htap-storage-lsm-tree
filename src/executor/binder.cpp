#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "executor/error.hpp"
#include "executor/binder.hpp"
#include "storage/model/schema_builder.hpp"

namespace htap::executor {

Binder::Binder(const storage::IStorageEngine& storage_engine)
    : storage_engine_(storage_engine) {
}

std::unique_ptr<BoundStatement> Binder::Bind(const parser::Statement& statement) const {
    if (const auto* create_table = dynamic_cast<const parser::CreateTableStatement*>(&statement)) {
        return BindCreateTable(*create_table);
    }

    if (const auto* insert = dynamic_cast<const parser::InsertStatement*>(&statement)) {
        return BindInsert(*insert);
    }

    if (const auto* select = dynamic_cast<const parser::SelectStatement*>(&statement)) {
        return BindSelect(*select);
    }

    throw InvalidQueryError("Unsupported statement type");    
}

storage::ValueType Binder::ParserDataTypeToStorageValueType(parser::DataType type) const {
    switch (type) {
        case parser::DataType::Int64:
            return storage::ValueType::INT64;
        case parser::DataType::Double:
            return storage::ValueType::DOUBLE;
        case parser::DataType::String:
            return storage::ValueType::STRING;
    }

    throw InvalidQueryError("Unsupported data type");
}

const storage::Schema& Binder::GetSchema(const std::string& table_name) const  {
    if (!storage_engine_.table_exists(table_name)) {
        throw TableNotFoundError(table_name);
    }

    return storage_engine_.get_table_schema(table_name);
}

ExpressionType Binder::GetAggregateResultType(parser::AggregateKind kind, ExpressionType argument_type) const {
    switch (kind) {
        case parser::AggregateKind::Count:
            return ExpressionType::Int64;

        case parser::AggregateKind::Avg:
            if (argument_type == ExpressionType::Int64 || argument_type == ExpressionType::Double) {
                return ExpressionType::Double;
            }
            throw InvalidQueryError("AVG supports only INT64 and DOUBLE");

        case parser::AggregateKind::Max:
            if (argument_type == ExpressionType::Int64 || argument_type == ExpressionType::Double) {
                return argument_type;
            }
            throw InvalidQueryError("MAX supports only INT64 and DOUBLE");

        case parser::AggregateKind::Min:
            if (argument_type == ExpressionType::Int64 || argument_type == ExpressionType::Double) {
                return argument_type;
            }
            throw InvalidQueryError("MIN supports only INT64 and DOUBLE");

        case parser::AggregateKind::Sum:
            if (argument_type == ExpressionType::Int64 || argument_type == ExpressionType::Double) {
                return argument_type;
            }
            throw InvalidQueryError("SUM supports only INT64 and DOUBLE");
    }

    throw InvalidQueryError("Unsupported aggregate kind");
}

storage::NullableValue Binder::ConvertLiteralExpression(const parser::Expression& expression) const {
    if (const auto* null_literal = dynamic_cast<const parser::NullLiteral*>(&expression)) {
        return std::nullopt;
    }
    if (const auto* int_literal = dynamic_cast<const parser::IntLiteral*>(&expression)) {
        return int_literal->value;
    }
    if (const auto* double_literal = dynamic_cast<const parser::DoubleLiteral*>(&expression)) {
        return double_literal->value;
    }
    if (const auto* string_literal = dynamic_cast<const parser::StringLiteral*>(&expression)) {
        return string_literal->value;
    }

    throw InvalidQueryError("Expected literal expression");
}

std::unique_ptr<BoundExpression> Binder::BindExpression(const parser::Expression& expression,
    const storage::Schema& schema) const {

    if (const auto* null_literal = dynamic_cast<const parser::NullLiteral*>(&expression)) {
        return std::make_unique<BoundLiteralExpression>(std::nullopt, ExpressionType::Null);
    }

    if (const auto* int_literal = dynamic_cast<const parser::IntLiteral*>(&expression)) {
        return std::make_unique<BoundLiteralExpression>(int_literal->value, ExpressionType::Int64);
    }

    if (const auto* double_literal = dynamic_cast<const parser::DoubleLiteral*>(&expression)) {
        return std::make_unique<BoundLiteralExpression>(double_literal->value, ExpressionType::Double);
    }

    if (const auto* string_literal = dynamic_cast<const parser::StringLiteral*>(&expression)) {
        return std::make_unique<BoundLiteralExpression>(string_literal->value, ExpressionType::String);
    }

    if (const auto* column_expression = dynamic_cast<const parser::ColumnExpression*>(&expression)) {
        std::optional<std::size_t> column_index = schema.get_column_index(column_expression->column_name);
        if (!column_index.has_value()) {
            throw ColumnNotFoundError(column_expression->column_name);
        }

        const storage::Column& column = schema.get_column(*column_index);

        return std::make_unique<BoundColumnExpression>(*column_index, column.type);
    }

    if (const auto* unary_expression = dynamic_cast<const parser::UnaryExpression*>(&expression)) {
        return std::make_unique<BoundUnaryExpression>(
            unary_expression->operation,
            BindExpression(*unary_expression->expression, schema)
        );
    }

    if (const auto* binary_expression = dynamic_cast<const parser::BinaryExpression*>(&expression)) {
        return std::make_unique<BoundBinaryExpression>(
            binary_expression->operation,
            BindExpression(*binary_expression->left, schema),
            BindExpression(*binary_expression->right, schema)
        );
    }

    if (const auto* is_null_expression = dynamic_cast<const parser::IsNullExpression*>(&expression)) {
        return std::make_unique<BoundIsNullExpression>(
            BindExpression(*is_null_expression->expression, schema),
            is_null_expression->is_not
        );
    }

    throw InvalidQueryError("Unsupported expression type");
}

std::vector<std::unique_ptr<BoundSelectItem>> Binder::BindSelectItem(const parser::SelectItem& item,
    const storage::Schema& schema) const {

    if (dynamic_cast<const parser::SelectItemStar*>(&item) != nullptr) {
        std::vector<std::unique_ptr<BoundSelectItem>> result;
        result.reserve(schema.size());

        for (std::size_t i = 0; i < schema.size(); ++i) {
            auto expression = std::make_unique<BoundColumnExpression>(
                i,
                schema.get_column(i).type
            );

            result.push_back(
                std::make_unique<BoundSelectItemExpression>(std::move(expression))
            );
        }

        return result;
    }

    if (const auto* expression_item =
            dynamic_cast<const parser::SelectItemExpression*>(&item)) {
        std::vector<std::unique_ptr<BoundSelectItem>> result;
        result.push_back(
            std::make_unique<BoundSelectItemExpression>(
                BindExpression(*expression_item->expression, schema)
            )
        );
        return result;
    }

    if (const auto* aggregate_item =
            dynamic_cast<const parser::SelectAggregateExpression*>(&item)) {
        auto argument = BindExpression(*aggregate_item->expression, schema);
        ExpressionType result_type = GetAggregateResultType(
            aggregate_item->kind,
            argument->type
        );

        std::vector<std::unique_ptr<BoundSelectItem>> result;
        result.push_back(
            std::make_unique<BoundSelectAggregateExpression>(
                aggregate_item->kind,
                std::move(argument),
                result_type
            )
        );
        return result;
    }

    throw InvalidQueryError("Unsupported select item");
}

BoundOrderByItem Binder::BindOrderByItem(const parser::OrderByItem& item,const storage::Schema& schema) const {

    BoundOrderByItem bound_item;
    bound_item.expression = BindExpression(*item.expression, schema);
    bound_item.direction = item.direction;

    return bound_item;
}

std::unique_ptr<BoundCreateTableStatement> Binder::BindCreateTable(
    const parser::CreateTableStatement& statement) const {
    try {
        storage::SchemaBuilder builder;

        for (const parser::ColumnDefinition& column : statement.columns) {
            builder.add_column(
                column.name,
                ParserDataTypeToStorageValueType(column.type),
                column.is_key,
                column.nullable
            );
        }

        std::unique_ptr<BoundCreateTableStatement> bound = std::make_unique<BoundCreateTableStatement>();
        bound->table_name = statement.table_name;
        bound->schema = builder.build();

        return bound;

    } catch (const std::exception& e) {
        throw InvalidQueryError(e.what());
    }
}

std::unique_ptr<BoundInsertStatement> Binder::BindInsert(const parser::InsertStatement& statement) const {
    
    const storage::Schema& schema = GetSchema(statement.table_name);

    if (statement.column_names.size() != statement.values.size()) {
        throw InvalidQueryError("The number of insert values does not match the number of columns");
    }

    storage::Row row_values(schema.size(), std::nullopt);
    std::vector<bool> assigned(schema.size(), false);

    for (std::size_t i = 0; i < statement.column_names.size(); ++i) {
        const std::string& column_name = statement.column_names[i];

        std::optional<std::size_t> column_index = schema.get_column_index(column_name);
        if (!column_index.has_value()) {
            throw ColumnNotFoundError(column_name);
        }

        if (assigned[*column_index]) {
            throw InvalidQueryError("Duplicate insert column: " + column_name);
        }

        row_values[*column_index] = ConvertLiteralExpression(*statement.values[i]);
        assigned[*column_index] = true;
    }

    for (std::size_t i = 0; i < schema.size(); ++i) {
        if (!schema.is_valid_value(i, row_values[i])) {
            throw TypeMismatchError("Invalid value for column: " + schema.get_column(i).name);
        }
    }

    std::unique_ptr<BoundInsertStatement> bound = std::make_unique<BoundInsertStatement>();
    bound->row_values = std::move(row_values);
    bound->schema = &schema;
    bound->table_name = statement.table_name;

    return bound;
}

std::unique_ptr<BoundSelectStatement> Binder::BindSelect(const parser::SelectStatement& statement) const {
    const storage::Schema& schema = GetSchema(statement.table_name);

    auto bound = std::make_unique<BoundSelectStatement>();
    bound->table_name = statement.table_name;
    bound->schema = &schema;
    bound->limit = statement.limit;

    for (const auto& select_item : statement.select_items) {
        std::vector<std::unique_ptr<BoundSelectItem>> items = BindSelectItem(*select_item, schema);

        for (auto& item : items) {
            bound->select_items.push_back(std::move(item));
        }
    }

    if (statement.where_expression) {
        bound->where_expression = BindExpression(*statement.where_expression, schema);
    }

    for (const auto& group_by_item : statement.group_by_items) {
        bound->group_by_items.push_back(BindExpression(*group_by_item, schema));
    }

    for (const auto& order_by_item : statement.order_by_items) {
        bound->order_by_items.push_back(BindOrderByItem(order_by_item, schema));
    }

    ValidateWhere(*bound);
    ValidateGroupedSelect(*bound);

    return bound;

}

bool Binder::HasAggregates(const BoundSelectStatement& statement) const {
    for (const auto& select_item : statement.select_items) {
        if (dynamic_cast<const BoundSelectAggregateExpression*>(select_item.get()) != nullptr) {
            return true;
        }
    }

    return false;
}

void Binder::ValidateWhere(const BoundSelectStatement& statement) const {
    if (statement.where_expression && statement.where_expression->type != ExpressionType::Boolean) {
        throw InvalidQueryError("WHERE expression must be BOOLEAN");
    }
}

bool Binder::AreEquivalentExpressions(const BoundExpression& left, const BoundExpression& right) const {
    if (left.type != right.type) {
        return false;
    }

    if (const auto* left_literal = dynamic_cast<const BoundLiteralExpression*>(&left)) {
        const auto* right_literal = dynamic_cast<const BoundLiteralExpression*>(&right);
        if (right_literal == nullptr) {
            return false;
        }

        return left_literal->value == right_literal->value;
    }

    if (const auto* left_column = dynamic_cast<const BoundColumnExpression*>(&left)) {
        const auto* right_column = dynamic_cast<const BoundColumnExpression*>(&right);
        if (right_column == nullptr) {
            return false;
        }

        return left_column->column_index == right_column->column_index;
    }

    if (const auto* left_unary = dynamic_cast<const BoundUnaryExpression*>(&left)) {
        const auto* right_unary = dynamic_cast<const BoundUnaryExpression*>(&right);
        if (right_unary == nullptr) {
            return false;
        }

        return left_unary->operation == right_unary->operation &&
               AreEquivalentExpressions(*left_unary->expression, *right_unary->expression);
    }

    if (const auto* left_binary = dynamic_cast<const BoundBinaryExpression*>(&left)) {
        const auto* right_binary = dynamic_cast<const BoundBinaryExpression*>(&right);
        if (right_binary == nullptr) {
            return false;
        }

        return left_binary->operation == right_binary->operation &&
               AreEquivalentExpressions(*left_binary->left, *right_binary->left) &&
               AreEquivalentExpressions(*left_binary->right, *right_binary->right);
    }

    if (const auto* left_is_null = dynamic_cast<const BoundIsNullExpression*>(&left)) {
        const auto* right_is_null = dynamic_cast<const BoundIsNullExpression*>(&right);
        if (right_is_null == nullptr) {
            return false;
        }

        return left_is_null->is_not == right_is_null->is_not &&
               AreEquivalentExpressions(*left_is_null->expression, *right_is_null->expression);
    }

    return false;
}

void Binder::ValidateGroupedSelect(const BoundSelectStatement& statement) const {
    const bool has_aggregates = HasAggregates(statement);

    if (!has_aggregates && statement.group_by_items.empty()) {
        return;
    }

    if (has_aggregates && statement.group_by_items.empty()) {
        for (const auto& select_item : statement.select_items) {
            if (dynamic_cast<const BoundSelectAggregateExpression*>(select_item.get()) == nullptr) {
                throw InvalidQueryError(
                    "Non-aggregate SELECT item is not allowed without GROUP BY when aggregates are present"
                );
            }
        }

        return;
    }

    for (const auto& select_item : statement.select_items) {
        const auto* expression_item = dynamic_cast<const BoundSelectItemExpression*>(select_item.get());
        if (expression_item == nullptr) {
            continue;
        }

        bool found_in_group_by = false;
        for (const auto& group_by_item : statement.group_by_items) {
            if (AreEquivalentExpressions(*expression_item->expression, *group_by_item)) {
                found_in_group_by = true;
                break;
            }
        }

        if (!found_in_group_by) {
            throw InvalidQueryError(
                "Non-aggregate SELECT item must appear in GROUP BY"
            );
        }
    }
}

}