#include "storage/model/schema_builder.hpp"

#include <stdexcept>

using namespace htap::storage;

SchemaBuilder::SchemaBuilder() : key_seen_(false) {}

SchemaBuilder& SchemaBuilder::add_column(std::string name, ValueType type, bool is_key, bool nullable) {
    if (column_names_.contains(name))
        throw std::invalid_argument("Duplicate column name: " + name);
    
    if (is_key) {
        if (type != ValueType::INT64)
            throw std::runtime_error("Primary key must be INT64");

        if (nullable)
            throw std::runtime_error("Primary key must not be nullable");

        if (key_seen_)
            throw std::runtime_error("Schema must contain only one primary key");
        
        if (columns_.size() != KEY_COLUMN_INDEX)
            throw std::runtime_error("Primary key must be at column index " + std::to_string(KEY_COLUMN_INDEX));
        
        key_seen_ = true;
    }

    column_names_.insert(name);

    columns_.push_back(Column{
        .name = std::move(name),
        .type = type,
        .is_key = is_key,
        .nullable = nullable
    });

    return *this;
}

Schema SchemaBuilder::build() {
    if (columns_.empty())
        throw std::runtime_error("Schema must contain at least one column");

    if (!key_seen_)
        throw std::runtime_error("Schema must contain exactly one primary key");

    return Schema(columns_);
}
