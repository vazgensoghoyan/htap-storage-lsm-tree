#include "storage/mock/mock_cursor.hpp"

#include <stdexcept>

using namespace htap::storage;

MockCursor::MockCursor(const std::map<Key, Row>* data, OptKey from, OptKey to) : data_(data), to_(to) {
    if (!data_)
        throw std::runtime_error("Null data pointer");

    if (from.has_value())
        it_ = data_->lower_bound(*from);
    else
        it_ = data_->begin();

    end_ = data_->end();
}

bool MockCursor::valid() const {
    if (it_ == end_)
        return false;

    if (to_.has_value() && it_->first >= *to_)
        return false;

    return true;
}

void MockCursor::next() {
    if (!valid()) return;
    ++it_;
}

Key MockCursor::key() const {
    if (!valid())
        throw std::runtime_error("Cursor invalid");

    return it_->first;
}

const Row& MockCursor::row() const {
    if (!valid())
        throw std::runtime_error("Cursor invalid");

    return it_->second;
}

NullableValue MockCursor::value(size_t column_idx) const {
    const auto& r = row();

    if (column_idx >= r.size())
        throw std::out_of_range("Column index out of range");

    return r[column_idx];
}
