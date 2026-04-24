#include "lsmtree/mem/cursors/imm_memtable_cursor.hpp"

#include <stdexcept>

using namespace htap::lsmtree;
using namespace htap::storage;

ImmutableMemTableCursor::ImmutableMemTableCursor(
    const std::vector<Row>* data,
    size_t start,
    size_t end,
    std::vector<size_t> projection
)
    : data_(data),
      idx_(start),
      end_(end),
      projection_(std::move(projection))
{
    if (!data_ || idx_ >= end_) {
        current_ = nullptr;
        return;
    }

    current_ = &(*data_)[idx_];
}

bool ImmutableMemTableCursor::valid() const {
    return current_ != nullptr && idx_ < end_;
}

void ImmutableMemTableCursor::next() {
    if (!valid()) return;

    ++idx_;

    if (idx_ >= end_) {
        current_ = nullptr;
        return;
    }

    current_ = &(*data_)[idx_];
}

Key ImmutableMemTableCursor::key() const {
    if (!valid())
        throw std::runtime_error("Cursor not valid");

    // инвариант: key всегда в column 0
    return std::get<Key>((*current_)[0].value());
}

NullableValue ImmutableMemTableCursor::value(size_t column_idx) const {
    if (!valid())
        throw std::runtime_error("Cursor not valid");

    if (!is_projected(column_idx))
        throw std::runtime_error("Column not in projection");

    return (*current_)[column_idx];
}

const Row& ImmutableMemTableCursor::row() const {
    return *current_;
}

bool ImmutableMemTableCursor::is_projected(size_t column_idx) const {
    if (projection_.empty())
        return true;

    for (auto c : projection_)
        if (c == column_idx)
            return true;

    return false;
}
