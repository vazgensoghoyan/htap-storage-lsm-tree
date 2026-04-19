#include <memory>
#include <stdexcept>

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

std::unique_ptr<BoundInsertStatement> Binder::BindInsert(
    const parser::InsertStatement& statement) const {
    
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

std::unique_ptr<BoundSelectStatement> Binder::BindSelect(
    const parser::SelectStatement& statement) const {
    throw InvalidQueryError("BindSelect is not implemented yet");
}

}