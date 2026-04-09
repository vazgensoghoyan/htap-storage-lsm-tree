#include "storage/model/row.hpp"

#include <stdexcept>

namespace htap::storage {

Row::Row(size_t column_count) : values_(column_count)
{}

void Row::set_key(int64_t key) {
    key_ = key;
}

int64_t Row::key() const {
    return key_;
}

void Row::set_value(size_t column_index, const NullableValue& value) {
    if (column_index >= values_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    values_[column_index] = value;
}

const NullableValue& Row::get_value(size_t column_index) const {
    if (column_index >= values_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return values_[column_index];
}

bool Row::has_value(size_t column_index) const {
    if (column_index >= values_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return values_[column_index].has_value();
}

size_t Row::size() const {
    return values_.size();
}

} // namespace htap::storage
