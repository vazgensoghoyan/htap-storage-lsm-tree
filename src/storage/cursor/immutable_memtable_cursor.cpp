#include "storage/cursor/immutable_memtable_cursor.hpp"

#include <cassert>
#include <stdexcept>
#include <variant>

namespace htap::storage::cursor { 

ImmutableMemTableCursor::ImmutableMemTableCursor(Iterator begin, Iterator end)
    : current_(begin), end_(end) {
}

bool ImmutableMemTableCursor::valid() const {
    return current_ != end_;
}

void ImmutableMemTableCursor::next() {
    if (valid()) {
        ++current_;
    }
}

Key ImmutableMemTableCursor::key() const {
    if (!valid()) {
        throw std::logic_error("ImmutableMemTableCursor::key() called on invalid cursor");
    }

    const Row& row = *current_;

    assert(KEY_COLUMN_INDEX < row.size());
    assert(row[KEY_COLUMN_INDEX].has_value());
    assert(std::holds_alternative<Key>(*row[KEY_COLUMN_INDEX]));

    return std::get<Key>(*row[KEY_COLUMN_INDEX]);
}

NullableValue ImmutableMemTableCursor::value(std::size_t column_idx) const {
    if (!valid()) {
        throw std::logic_error("ImmutableMemTableCursor::value() called on invalid cursor");
    }

    const Row& row = *current_;

    if (column_idx >= row.size()) {
        throw std::out_of_range("Column index out of range");
    }

    return row[column_idx];
}
    
}