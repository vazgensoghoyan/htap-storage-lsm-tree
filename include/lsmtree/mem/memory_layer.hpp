#pragma once // lsmtree/mem/memory_layer.hpp

#include <deque>
#include <memory>

#include "storage/api/types.hpp"

#include "lsmtree/mem/memtable.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

class MemoryLayer {
public:
    // shared чтобы worker мог flush-ить и cursor одновременно использовать
    using ImmPtr = std::shared_ptr<ImmutableMemTable>;

    MemoryLayer(size_t threshold);

    void insert(const storage::Row& row);

    void active_to_immutable();

    size_t immutable_count() const;

    ImmPtr front_immutable() const;

    ImmPtr pop_front_immutable();

    const MemTable& active() const noexcept;

    const std::deque<ImmPtr>& immutables() const noexcept;

private:
    size_t threshold_;

    std::unique_ptr<MemTable> active_;
    std::deque<ImmPtr> immutables_;
};

} // namespace htap::lsmtree
