#include "storage/mock/mock_cursor.hpp"

#include <algorithm>
#include <stdexcept>

using namespace htap::storage;

MockCursor::MockCursor(
    std::vector<Key> keys,
    const std::map<Key, Row>* data,
    const std::vector<size_t>& pr
) : keys_(std::move(keys)), data_(data), proj_(pr), pos_(0) { }

bool MockCursor::valid() const {
    return pos_ < keys_.size();
}

void MockCursor::next() {
    if (pos_ < keys_.size())
        ++pos_;
}

Key MockCursor::key() const {
    return keys_[pos_];
}

NullableValue MockCursor::value(size_t column_idx) const {
    const auto& row = data_->at(keys_[pos_]);

    if (!is_projected(column_idx))
        throw std::runtime_error("Column not in projection");

    return row[column_idx];
}

const Row& MockCursor::row() const {
    return data_->at(keys_[pos_]);
}

bool MockCursor::is_projected(size_t column_idx) const {
    return std::find(proj_.begin(), proj_.end(), column_idx) != proj_.end();
}
