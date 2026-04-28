#include "lsmtree/mem/cursors/merge_cursor.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

MergeCursor::MergeCursor(std::vector<std::unique_ptr<ICursor>> cursors)
    : cursors_(std::move(cursors))
    , current_valid_(false)
    , current_idx_(0)
    , current_key_(0) {

    for (size_t i = 0; i < cursors_.size(); ++i) {
        if (cursors_[i] && cursors_[i]->valid()) {
            heap_.push(HeapEntry{i, cursors_[i]->key()});
        }
    }

    advance_to_next_valid();
}

bool MergeCursor::valid() const {
    return current_valid_;
}

void MergeCursor::next() {
    if (!current_valid_) return;

    size_t idx = current_idx_;
    cursors_[idx]->next();

    if (cursors_[idx]->valid()) {
        heap_.push(HeapEntry{idx, cursors_[idx]->key()});
    }

    advance_to_next_valid();
}

Key MergeCursor::key() const {
    return current_key_;
}

NullableValue MergeCursor::value(size_t column_idx) const {
    return cursors_[current_idx_]->value(column_idx);
}

bool MergeCursor::HeapEntry::operator>(const HeapEntry& other) const {
    if (key != other.key)
        return key > other.key;

    return cursor_idx > other.cursor_idx;
}

void MergeCursor::advance_to_next_valid() {
    if (heap_.empty()) {
        current_valid_ = false;
        return;
    }

    HeapEntry top = heap_.top();
    heap_.pop();

    current_idx_ = top.cursor_idx;
    current_key_ = top.key;
    current_valid_ = true;

    while (!heap_.empty() && heap_.top().key == current_key_) {
        auto dup = heap_.top();
        heap_.pop();

        cursors_[dup.cursor_idx]->next();

        if (cursors_[dup.cursor_idx]->valid()) {
            heap_.push(HeapEntry{
                dup.cursor_idx,
                cursors_[dup.cursor_idx]->key()
            });
        }
    }
}
