#include "storage/cursor/merge_cursor.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace htap::storage::cursor {

MergeCursor::MergeCursor(std::vector<std::unique_ptr<ICursor>> cursors) 
    : cursors_(std::move(cursors)) {
    for (std::size_t i = 0; i < cursors_.size(); ++i) {
        push_if_valid(i);
    }
}

bool MergeCursor::valid() const {
    return !heap_.empty();
}

void MergeCursor::next() {
    if (!valid()) return;

    std::size_t pop_cursor = heap_.top().second;
    heap_.pop();

    cursors_[pop_cursor]->next();

    push_if_valid(pop_cursor);
}

Key MergeCursor::key() const {
    if (!valid()) {
        throw std::logic_error("MergeCursor::key() called on invalid cursor");
    }

    return heap_.top().first;
}

NullableValue MergeCursor::value(std::size_t column_idx) const {
    if (!valid()) {
        throw std::logic_error("MergeCursor::value() called on invalid cursor");
    }

    std::size_t current_cursor = heap_.top().second;

    return cursors_[current_cursor]->value(column_idx);
}

void MergeCursor::push_if_valid(std::size_t cursor_index) {
    if (cursors_[cursor_index]->valid()) {
        Key current_key = cursors_[cursor_index]->key();
        heap_.push(std::make_pair(current_key, cursor_index));
    }
}


}