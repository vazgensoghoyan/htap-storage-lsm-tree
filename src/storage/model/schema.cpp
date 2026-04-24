#include "storage/model/schema.hpp"

using namespace htap::storage;

Schema::Schema(std::vector<Column> columns, size_t key_index)
    : columns_(std::move(columns)),
      key_index_(key_index)
{
    for (size_t i = 0; i < columns_.size(); ++i)
        name_to_index_[columns_[i].name] = i;
}

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

std::optional<size_t> Schema::get_column_index(const std::string& name) const noexcept {
    auto it = name_to_index_.find(name);

    if (it == name_to_index_.end())
        return std::nullopt;

    return it->second;
}

size_t Schema::key_column_index() const noexcept {
    return key_index_;
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
