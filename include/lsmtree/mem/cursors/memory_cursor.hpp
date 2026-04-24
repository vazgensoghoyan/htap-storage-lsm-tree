#pragma once // lsmtree/mem/cursors/memory_cursor.hpp

#include <vector>
#include <memory>
#include <optional>
#include <queue>

#include "storage/api/cursor_interface.hpp"
#include "storage/api/types.hpp"

#include "lsmtree/mem/memtable.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

class MemoryCursor final : public storage::ICursor {
public:
    MemoryCursor(
        std::shared_ptr<const MemTable> active,
        std::vector<std::shared_ptr<const ImmutableMemTable>> immutables,
        storage::OptKey from,
        storage::OptKey to,
        std::vector<size_t> projection
    );

    bool valid() const override;
    void next() override;

    storage::Key key() const override;
    storage::NullableValue value(size_t column_idx) const override;
    const storage::Row& row() const override;

private:
    struct Source {
        std::unique_ptr<storage::ICursor> cursor;
        size_t priority; // 0 = active, больше = older
        bool valid = false;
    };

    struct HeapItem {
        storage::Key key;
        size_t source_idx;

        bool operator>(const HeapItem& other) const {
            return key > other.key;
        }
    };

private:
    void init_sources(
        const std::shared_ptr<const MemTable>& active,
        const std::vector<std::shared_ptr<const ImmutableMemTable>>& immutables,
        storage::OptKey from
    );

    void push_source(size_t idx);
    void advance();

    bool should_skip_current() const;

private:
    std::priority_queue<
        HeapItem,
        std::vector<HeapItem>,
        std::greater<>
    > heap_;

    std::vector<size_t> projection_;
    storage::OptKey to_;
    std::vector<Source> sources_;

    storage::Key current_key_{};
    const storage::Row* current_row_ = nullptr;

    bool valid_ = false;
};

} // namespace htap::lsmtree
