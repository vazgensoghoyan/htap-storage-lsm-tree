#include "storage/model/schema.hpp"

using namespace htap::storage;

Schema::Schema(std::vector<Column> columns) : columns_(std::move(columns)) {}

const std::vector<Column>& Schema::columns() const noexcept {
    return columns_;
}

const Column& Schema::get_column(size_t index) const {
    if (index >= columns_.size())
        throw std::out_of_range("Column index out of range");

    return columns_[index];
}

size_t Schema::size() const noexcept {
    return columns_.size();
}

bool Schema::is_valid_value(size_t column_index, const NullableValue& value) const {
    if (column_index >= columns_.size())
        return false;

    const auto& col = columns_[column_index];

    // NULL case
    if (!value.has_value())
        return col.nullable;

    // Type check
    const Value& v = value.value();

    switch (col.type) {
        case ValueType::INT64:
            return std::holds_alternative<int64_t>(v);

        case ValueType::DOUBLE:
            return std::holds_alternative<double>(v);

        case ValueType::STRING:
            return std::holds_alternative<std::string>(v);
    }

    return false; // до сюда не дойдем
}
