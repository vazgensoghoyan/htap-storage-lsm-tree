#include "storage/mock/mock_cursor.hpp"

#include <algorithm>
#include <stdexcept>

using namespace htap::storage;

MockCursor::MockCursor(
    std::vector<Key> keys,
    const std::map<Key, Row>* data,
    const std::vector<size_t>& projection
)
    : keys_(std::move(keys))
    , data_(data)
    , projection_(projection)
    , pos_(0)
{
    advance_to_valid();
}

bool MockCursor::valid() const {
    return pos_ < keys_.size();
}

void MockCursor::next() {
    if (pos_ < keys_.size()) {
        ++pos_;
        advance_to_valid();
    }
}

void MockCursor::seek(const Key& key) {
    pos_ = std::lower_bound(keys_.begin(), keys_.end(), key) - keys_.begin();
    advance_to_valid();
}

Key MockCursor::key() const {
    return keys_[pos_];
}

const NullableValue& MockCursor::value(size_t column_idx) const {
    const auto& row = data_->at(keys_[pos_]);

    if (!is_projected(column_idx)) {
        throw std::runtime_error("Column not in projection");
    }

    return row[column_idx];
}

bool MockCursor::is_projected(size_t column_idx) const {
    return std::find(
        projection_.begin(),
        projection_.end(),
        column_idx
    ) != projection_.end();
}

void MockCursor::advance_to_valid() {
    while (pos_ < keys_.size()) {
        if (data_->find(keys_[pos_]) != data_->end()) {
            break;
        }
        ++pos_;
    }
}
