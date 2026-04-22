#pragma once // lsmtree/mem/memory_cursor.hpp

#include <vector>
#include <memory>
#include <optional>
#include <queue>

#include "storage/api/cursor_interface.hpp"
#include "lsmtree/mem/memtable.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

/**
 * MemoryCursor
 *
 * Forward-only итератор по memory layer:
 * - active memtable
 * - immutable memtables (какое то колво)
 *
 * Гарантии:
 * - возвращает ключи в отсортированном порядке
 * - устраняет дубликаты (берёт самую новую версию)
 * - snapshot consistency (через shared_ptr)
 * - Не копирует данные
 */
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

    ~MemoryCursor() override = default;

    bool valid() const override;
    void next() override;

    Key key() const override;
    NullableValue value(size_t column_idx) const override;

private:
    // Один источник данных (memtable или immutable)
    struct Source {
        MemIt it;
        MemIt end;
        size_t priority; // меньше = новее (active самый приоритетный)
    };

    // Элемент в куче (k-way merge)
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

    void build_heap();
    void advance();              // перейти к следующему валидному ключу
    void skip_duplicates(Key k); // пропустить старые версии

    bool is_projected(size_t column_idx) const;

private:
    // projection (column-level фильтрация)
    std::vector<size_t> projection_;

    // верхняя граница range
    std::optional<Key> to_;

    // snapshot (гарантирует lifetime)
    std::shared_ptr<const MemTable> active_;
    std::vector<std::shared_ptr<const ImmutableMemTable>> immutables_;

    // источники (active + immutables)
    std::vector<Source> sources_;

    // min-heap по ключу
    std::priority_queue<
        HeapItem,
        std::vector<HeapItem>,
        std::greater<>
    > heap_;

    // текущее состояние
    Key current_key_;
    const Row* current_row_ = nullptr;

    bool valid_ = false;
};

} // namespace htap::lsmtree
