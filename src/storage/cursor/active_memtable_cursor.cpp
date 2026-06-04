#include "storage/cursor/active_memtable_cursor.hpp"

#include <stdexcept>

namespace htap::storage::cursor { 

ActiveMemTableCursor::ActiveMemTableCursor(Iterator begin, Iterator end)
    : current_(begin), end_(end) {
}

bool ActiveMemTableCursor::valid() const {
    return current_ != end_;
}

void ActiveMemTableCursor::next() {
    if (valid()) {
        ++current_;
    }
}

Key ActiveMemTableCursor::key() const {
    if (!valid()) {
        throw std::logic_error("ActiveMemTableCursor::key() called on invalid cursor");
    }

    return current_->first;
}

NullableValue ActiveMemTableCursor::value(std::size_t column_idx) const {
    if (!valid()) {
        throw std::logic_error("ActiveMemTableCursor::value() called on invalid cursor");
    }

    const Row& row = current_->second;

    if (column_idx >= row.size()) {
        throw std::out_of_range("Column index out of range");
    }

    return row[column_idx];
}
    
}