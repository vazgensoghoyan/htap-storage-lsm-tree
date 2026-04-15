#include "storage/model/schema_builder.hpp"

#include <stdexcept>

using namespace htap::storage;

SchemaBuilder& SchemaBuilder::add_column(
    std::string name,
    ValueType type,
    bool is_key,
    bool nullable) 
{
    if (column_names_.contains(name)) {
        throw std::invalid_argument("Duplicate column name: " + name);
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
    if (columns_.empty()) {
        throw std::runtime_error("Schema must contain at least one column");
    }

    size_t key_count = 0;
    size_t key_index = 0;

    for (size_t i = 0; i < columns_.size(); ++i) {
        const auto& col = columns_[i];

        if (col.is_key) {
            key_count++;
            key_index = i;
        }
    }

    if (key_count == 0) {
        throw std::runtime_error("Schema must contain exactly one primary key");
    }

    if (key_count > 1) {
        throw std::runtime_error("Schema must contain only one primary key");
    }

    const auto& key_col = columns_[key_index];

    if (key_col.type != ValueType::INT64) {
        throw std::runtime_error("Primary key must be INT64");
    }

    if (key_col.nullable) {
        throw std::runtime_error("Primary key must be non-nullable");
    }

    return Schema(columns_, key_index);
}
