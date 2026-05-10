#pragma once // lsmtree/mem/memory_layer.hpp

#include <vector>
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

    void insert(storage::Key key, const storage::Row& row);

    void force_freeze();

    size_t immutable_count() const;

private:
    size_t threshold_;

    std::unique_ptr<MemTable> active_;
    std::vector<std::unique_ptr<ImmutableMemTable>> immutables_;
};

} // namespace htap::lsmtree
