#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <algorithm>

#include "storage/api/cursor_interface.hpp"
#include "lsmtree/mem/memtable.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

class MemoryCursor final : public storage::ICursor {
public:
    using Key = storage::Key;
    using Row = storage::Row;
    using NullableValue = storage::NullableValue;

    using MemIt = MemTable::Iterator;
    using ImmIt = ImmutableMemTable::Iterator;

public:
    MemoryCursor(
        std::shared_ptr<const MemTable> active,
        std::vector<std::shared_ptr<const ImmutableMemTable>> immutables,
        std::optional<Key> from,
        std::optional<Key> to,
        std::vector<size_t> projection
    );

    bool valid() const override;
    void next() override;

    Key key() const override;
    NullableValue value(size_t column_idx) const override;


private:
    struct Source {
        MemIt it;
        MemIt end;
        size_t priority; // 0 = newest (active), больше = older
    };

    struct HeapItem {
        Key key;
        size_t source_idx;

        bool operator>(const HeapItem& other) const {
            return key > other.key;
        }
    };
private:
    void init_sources(
        const std::shared_ptr<const MemTable>& active,
        const std::vector<std::shared_ptr<const ImmutableMemTable>>& immutables,
        std::optional<Key> from
    );

    void push_if_valid(size_t source_idx);
    void advance();

    bool is_projected(size_t column_idx) const;

private:
    std::vector<size_t> projection_;
    std::optional<Key> to_;

    std::shared_ptr<const MemTable> active_;
    std::vector<std::shared_ptr<const ImmutableMemTable>> immutables_;

    std::vector<Source> sources_;

    std::priority_queue<
        HeapItem,
        std::vector<HeapItem>,
        std::greater<>
    > heap_;

    Key current_key_;
    const Row* current_row_ = nullptr;
    bool valid_ = false;
};

} // namespace htap::lsmtree
