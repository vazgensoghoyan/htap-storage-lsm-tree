#include "storage/mock/mock_cursor.hpp"

#include <stdexcept>

using namespace htap::storage;

MockCursor::MockCursor(
    std::vector<int64_t> keys,
    std::vector<std::vector<NullableValue>> rows,
    std::vector<size_t> projection)
    : keys_(std::move(keys)),
      rows_(std::move(rows)),
      projection_(std::move(projection))
{}

bool MockCursor::valid() const {
    return index_ < rows_.size();
}

void MockCursor::next() {
    if (index_ < rows_.size()) {
        ++index_;
    }
}

int64_t MockCursor::key() const {
    if (!valid()) {
        throw std::out_of_range("Cursor is not valid");
    }
    return keys_[index_];
}

bool MockCursor::is_null(size_t column_idx) const {
    if (!valid()) {
        throw std::out_of_range("Cursor is not valid");
    }

    if (column_idx >= projection_.size()) {
        throw std::out_of_range("Invalid column index");
    }

    size_t actual_idx = projection_[column_idx];
    return !rows_[index_][actual_idx].has_value();
}

const Value& MockCursor::value(size_t column_idx) const {
    if (!valid()) {
        throw std::out_of_range("Cursor is not valid");
    }

    if (column_idx >= projection_.size()) {
        throw std::out_of_range("Invalid column index");
    }

    size_t actual_idx = projection_[column_idx];
    const auto& opt = rows_[index_][actual_idx];

    if (!opt.has_value()) {
        throw std::runtime_error("Value is NULL");
    }

    return opt.value();
}
