#include "storage/mock/mock_cursor.hpp"

#include <algorithm>
#include <stdexcept>

using namespace htap::storage;

MockCursor::MockCursor(const MockStorage& data, std::vector<size_t> projection, Key from, Key to)
    : data_(data), projection_(std::move(projection))
{
    it_ = data_.lower_bound(from);
    end_ = data_.lower_bound(to);
}

bool MockCursor::valid() const {
    return it_ != end_;
}

void MockCursor::next() {
    if (it_ != end_) ++it_;
}

void MockCursor::seek(const Key& key) {
    it_ = data_.lower_bound(key);

    // если вышли за границу диапазона, делаем курсор невалидным
    if (it_ == data_.end() || it_->first >= (end_ == data_.end() ? KEY_MAX : end_->first)) {
        it_ = end_;
    }
}

Key MockCursor::key() const {
    if (!valid()) {
        throw std::runtime_error("Cursor is not valid");
    }
    return it_->first;
}

bool MockCursor::is_null(size_t column_idx) const {
    if (!valid()) {
        throw std::runtime_error("Cursor is not valid");
    }

    check_projection(column_idx);

    const auto& row = it_->second;

    if (column_idx >= row.size()) {
        throw std::runtime_error("Column index out of bounds");
    }

    return !row[column_idx].has_value();
}

const Value& MockCursor::value(size_t column_idx) const {
    if (!valid())
        throw std::runtime_error("Cursor is not valid");

    check_projection(column_idx);

    const auto& row = it_->second;

    if (column_idx >= row.size())
        throw std::runtime_error("Column index out of bounds");

    if (!row[column_idx].has_value())
        throw std::runtime_error("Value is NULL");

    return row[column_idx].value();
}

void MockCursor::check_projection(size_t column_idx) const {
    if (std::find(projection_.begin(), projection_.end(), column_idx) == projection_.end())
        throw std::runtime_error("Column not in projection");
}
