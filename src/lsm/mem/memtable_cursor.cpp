#include "lsmtree/mem/memtable_cursor.hpp"

#include <stdexcept>

using namespace htap::lsmtree;
using namespace htap::storage;

MemTableCursor::MemTableCursor(
    const Map* data,
    OptKey from,
    OptKey to,
    std::vector<size_t> projection
)
    : data_(data),
      to_(to),
      projection_(std::move(projection))
{
    if (!data_) {
        it_ = end_ = {};
        current_row_ = nullptr;
        return;
    }

    // стартуем с from или с begin
    it_ = from ? data_->lower_bound(*from) : data_->begin();
    end_ = data_->end();

    // если сразу вышли за to
    if (it_ != end_ && to_ && it_->first >= *to_) {
        it_ = end_;
        current_row_ = nullptr;
        return;
    }

    if (it_ != end_) {
        current_row_ = &it_->second;
    } else {
        current_row_ = nullptr;
    }
}

bool MemTableCursor::valid() const {
    return it_ != end_ && current_row_ != nullptr;
}

void MemTableCursor::next() {
    if (!valid())
        return;

    ++it_;

    if (it_ == end_) {
        current_row_ = nullptr;
        return;
    }

    // проверка upper bound
    if (to_ && it_->first >= *to_) {
        it_ = end_;
        current_row_ = nullptr;
        return;
    }

    current_row_ = &it_->second;
}

Key MemTableCursor::key() const {
    if (!valid())
        throw std::runtime_error("MemTableCursor: invalid state");

    // инвариант: key всегда в Row[0]
    const auto& row = *current_row_;
    return std::get<Key>(row[0].value());
}

NullableValue MemTableCursor::value(size_t column_idx) const {
    if (!valid())
        throw std::runtime_error("MemTableCursor: invalid state");

    if (!is_projected(column_idx))
        throw std::runtime_error("MemTableCursor: column not in projection");

    return (*current_row_)[column_idx];
}

const Row& MemTableCursor::row() const {
    return *current_row_;
}

bool MemTableCursor::is_projected(size_t column_idx) const {
    if (projection_.empty())
        return true;

    for (auto c : projection_) {
        if (c == column_idx)
            return true;
    }

    return false;
}
