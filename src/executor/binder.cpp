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
    throw InvalidQueryError("BindInsert is not implemented yet");
}
std::unique_ptr<BoundSelectStatement> Binder::BindSelect(
    const parser::SelectStatement& statement) const {
    throw InvalidQueryError("BindSelect is not implemented yet");
}

}