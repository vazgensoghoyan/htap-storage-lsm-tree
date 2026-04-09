#include "storage/model/schema.hpp"

namespace htap::storage {

void Schema::add_column(
    const std::string& name,
    ValueType type,
    bool is_key,
    bool nullable)
{
    if (name_to_index_.count(name) > 0) {
        throw std::invalid_argument("Duplicate column name: " + name);
    }

    if (is_key && key_index_.has_value()) {
        throw std::invalid_argument("Multiple primary keys are not allowed");
    }

    size_t index = columns_.size();

    columns_.push_back( Column{ name, type, is_key, nullable } );

    name_to_index_[name] = index;

    if (is_key) {
        key_index_ = index;
    }
}

const std::vector<Column>& Schema::columns() const {
    return columns_;
}

size_t Schema::size() const {
    return columns_.size();
}

std::optional<size_t> Schema::get_column_index(const std::string& name) const {
    auto it = name_to_index_.find(name);
    if (it == name_to_index_.end()) {
        return std::nullopt;
    }
    return it->second;
}

const Column& Schema::get_column(size_t index) const {
    if (index >= columns_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return columns_[index];
}

size_t Schema::key_column_index() const {
    if (!key_index_.has_value()) {
        throw std::runtime_error("Primary key is not defined");
    }
    return key_index_.value();
}

bool Schema::is_valid_value(
    size_t column_index,
    const NullableValue& value) const
{
    if (column_index >= columns_.size()) {
        return false;
    }

    const Column& column = columns_[column_index];

    // NULL проверка
    if (!value.has_value()) {
        return column.nullable;
    }

    // тип должен совпадать
    const Value& v = value.value();

    switch (column.type) {
        case ValueType::INT64:
            return std::holds_alternative<int64_t>(v);
        case ValueType::DOUBLE:
            return std::holds_alternative<double>(v);
        case ValueType::STRING:
            return std::holds_alternative<std::string>(v);
        default:
            return false;
    }
}

} // namespace htap::storage
