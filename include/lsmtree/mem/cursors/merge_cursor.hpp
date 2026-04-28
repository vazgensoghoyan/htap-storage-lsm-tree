#pragma once // lsmtree/mem/cursors/merge_cursor.hpp

#include <vector>
#include <memory>
#include <queue>

#include "storage/api/cursor_interface.hpp"

namespace htap::lsmtree {

class MergeCursor : public storage::ICursor {
public:
    explicit MergeCursor(std::vector<std::unique_ptr<ICursor>> cursors);

    bool valid() const override;
    void next() override;

    storage::Key key() const override;
    storage::NullableValue value(size_t column_idx) const override;

private:
    struct HeapEntry {
        size_t cursor_idx;
        storage::Key key;

        bool operator>(const HeapEntry& other) const;
    };

    void advance_to_next_valid();

private:
    std::vector<std::unique_ptr<ICursor>> cursors_;

    std::priority_queue<
        HeapEntry,
        std::vector<HeapEntry>,
        std::greater<>
    > heap_;

    bool current_valid_;
    size_t current_idx_;
    storage::Key current_key_;
};

} // namespace htap::lsmtree
