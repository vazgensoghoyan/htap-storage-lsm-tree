#pragma once // lsmtree/mem/memory_layer.hpp

#include <vector>
#include <memory>
#include <optional>

#include "storage/api/types.hpp"
#include "storage/api/cursor_interface.hpp"

#include "lsmtree/mem/memtable.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

// колво записей после которых фризим
// потом схема другая будет, по памяти
inline constexpr size_t DEFAULT_MEMTABLE_THRESHOLD = 10000;

/**
 * MemoryLayer
 *
 * In-memory слой LSM:
 * - active MemTable (write, read)
 * - immutable MemTables (read-only)
 *
 * Гарантии:
 * - latest write wins
 * - scan возвращает отсортированные ключи без дубликатов
 */
class MemoryLayer {
public:
    MemoryLayer();

    void insert(storage::Key key, const storage::Row& row);

    std::optional<storage::Row> get(storage::Key key) const;

    std::unique_ptr<storage::ICursor> scan(
        std::optional<storage::Key> from,
        std::optional<storage::Key> to,
        std::vector<size_t> projection = {}) const;

    void force_freeze();

    size_t immutable_count() const noexcept;

private:
    size_t threshold_ = DEFAULT_MEMTABLE_THRESHOLD;

    std::shared_ptr<MemTable> active_;
    std::vector<std::shared_ptr<ImmutableMemTable>> immutables_;
};

} // namespace htap::lsmtree
