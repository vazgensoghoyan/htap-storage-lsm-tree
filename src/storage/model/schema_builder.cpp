#include "storage/model/schema_builder.hpp"

#include <stdexcept>

using namespace htap::storage;

SchemaBuilder& SchemaBuilder::add_column(std::string name, ValueType type, bool nullable) {
    if (column_names_.contains(name))
        throw std::invalid_argument("Duplicate column name: " + name);

    column_names_.insert(name);

    columns_.push_back(Column{
        .name = std::move(name),
        .type = type,
        .nullable = nullable
    });

    return *this;
}

Schema SchemaBuilder::build() {
    if (columns_.empty())
        throw std::runtime_error("Schema must contain at least one column");

    return Schema(columns_);
}
