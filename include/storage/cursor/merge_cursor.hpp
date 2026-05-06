#pragma once

#include "storage/api/cursor_interface.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

namespace htap::storage::cursor {

class MergeCursor final : public ICursor {
public:
    explicit MergeCursor(std::vector<std::unique_ptr<ICursor>> cursors);

    bool valid() const override;

    void next() override;

    Key key() const override;

    NullableValue value(std::size_t column_idx) const override;

private:
    void push_if_valid(std::size_t cursor_index);

private:
    using HeapItem = std::pair<Key, std::size_t>;

    std::vector<std::unique_ptr<ICursor>> cursors_;
    std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap_;

};

}