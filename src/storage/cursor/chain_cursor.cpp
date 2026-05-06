#include "storage/cursor/chain_cursor.hpp"

#include <stdexcept>
#include <utility>

namespace htap::storage::cursor {

ChainCursor::ChainCursor(std::vector<std::unique_ptr<ICursor>> cursors)
    : cursors_(std::move(cursors)) {
    skip_invalid_cursors();
}

bool ChainCursor::valid() const {
    return current_cursor_ < cursors_.size() && cursors_[current_cursor_]->valid();
}

void ChainCursor::next() {
    if (!valid()) return;

    cursors_[current_cursor_]->next();
    skip_invalid_cursors();
}

Key ChainCursor::key() const {
    if (!valid()) {
        throw std::logic_error("ChainCursor::key() called on invalid cursor");
    }

    return cursors_[current_cursor_]->key();
}

NullableValue ChainCursor::value(std::size_t column_idx) const {
    if (!valid()) {
        throw std::logic_error("ChainCursor::value() called on invalid cursor");
    }

    return cursors_[current_cursor_]->value(column_idx);
}

void ChainCursor::skip_invalid_cursors() {
    while (current_cursor_ < cursors_.size() && !cursors_[current_cursor_]->valid()) {
        ++current_cursor_;
    }
}

}