#pragma once // lsmtree/mem/memory_layer.hpp

#include <vector>
#include <memory>
#include <optional>

#include "storage/api/types.hpp"

#include "lsmtree/mem/memtable_interface.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

#include "lsmtree/mem/memtable_realizations/memtable_factory.hpp"

namespace htap::lsmtree {

inline constexpr size_t DEFAULT_MEMTABLE_THRESHOLD = 10000;

class MemoryLayer {
public:
    MemoryLayer(
        MemTableType type = MemTableType::MAP,
        size_t threshold = DEFAULT_MEMTABLE_THRESHOLD
    );

    void insert(const storage::Row& row);

    void force_freeze();

    size_t immutable_count() const;

private:
    MemTableType memtable_type_;
    size_t threshold_;

    std::unique_ptr<IMemTable> active_;
    std::vector<std::unique_ptr<ImmutableMemTable>> immutables_;
};

} // namespace htap::lsmtree
