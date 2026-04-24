#pragma once // lsmtree/mem/cursors/merge_cursor.hpp

#include <queue>
#include <vector>
#include <memory>
#include <functional>

#include "storage/api/cursor_interface.hpp"

namespace htap::lsmtree {

class MergeCursor : public storage::ICursor {
public:
    explicit MergeCursor(std::vector<std::unique_ptr<ICursor>> cursors)
        : cursors_(std::move(cursors)) {

        // инициализация heap
        for (size_t i = 0; i < cursors_.size(); ++i) {
            if (cursors_[i] && cursors_[i]->valid()) {
                heap_.push(HeapEntry{i, cursors_[i]->key()});
            }
        }

        advance_to_next_valid();
    }

    bool valid() const override {
        return current_valid_;
    }

    void next() override {
        if (!current_valid_) return;

        // двигаем текущий курсор
        size_t idx = current_idx_;
        cursors_[idx]->next();

        if (cursors_[idx]->valid()) {
            heap_.push(HeapEntry{idx, cursors_[idx]->key()});
        }

        advance_to_next_valid();
    }

    storage::Key key() const override {
        return current_key_;
    }

    storage::NullableValue value(size_t column_idx) const override {
        return cursors_[current_idx_]->value(column_idx);
    }

private:
    struct HeapEntry {
        size_t cursor_idx;
        storage::Key key;

        // min-heap
        bool operator>(const HeapEntry& other) const {
            if (key != other.key)
                return key > other.key;
            // tie-break: lower index = higher priority
            return cursor_idx > other.cursor_idx;
        }
    };

    std::vector<std::unique_ptr<ICursor>> cursors_;

    std::priority_queue<
        HeapEntry,
        std::vector<HeapEntry>,
        std::greater<>
    > heap_;

    bool current_valid_ = false;
    size_t current_idx_ = 0;
    storage::Key current_key_ = 0;

    void advance_to_next_valid() {
        if (heap_.empty()) {
            current_valid_ = false;
            return;
        }

        // берём минимальный ключ
        HeapEntry top = heap_.top();
        heap_.pop();

        current_idx_ = top.cursor_idx;
        current_key_ = top.key;
        current_valid_ = true;

        // дедупликация ключей
        while (!heap_.empty() && heap_.top().key == current_key_) {
            auto dup = heap_.top();
            heap_.pop();

            // продвигаем дубликатный курсор
            cursors_[dup.cursor_idx]->next();

            if (cursors_[dup.cursor_idx]->valid()) {
                heap_.push(HeapEntry{
                    dup.cursor_idx,
                    cursors_[dup.cursor_idx]->key()
                });
            }
        }
    }
};

} // namespace htap::lsmtree
