#pragma once // lsmtree/mem/memory_layer.hpp

#include <deque>
#include <memory>
#include <optional>

#include "storage/api/types.hpp"

#include "lsmtree/mem/memtable.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

inline constexpr size_t DEFAULT_MEMTABLE_THRESHOLD = 10000;

class MemoryLayer {
public:
    MemoryLayer(size_t threshold = DEFAULT_MEMTABLE_THRESHOLD);

    void insert(const storage::Row& row);

    void force_freeze();

    size_t immutable_count() const;

    std::unique_ptr<ImmutableMemTable> pop_immutable();

    const MemTable& active() const noexcept;

    const std::deque<std::unique_ptr<ImmutableMemTable>>& immutables() const noexcept;

private:
    size_t threshold_;

    std::unique_ptr<MemTable> active_;
    std::deque<std::unique_ptr<ImmutableMemTable>> immutables_;
};

} // namespace htap::lsmtree
