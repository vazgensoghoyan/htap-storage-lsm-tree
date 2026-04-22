#pragma once

#include <memory>
#include <vector>
#include <optional>

#include "storage/api/types.hpp"
#include "lsmtree/mem/memtable.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

// возможно сюда стоит добавить итератор по всем этим MemTable
// который несложно реализовать так, что он будет отсортированно проходить

inline constexpr size_t DEFAULT_MEMTABLE_THRESHOLD = 10000;

class MemoryLayer {
public:
    MemoryLayer(size_t memtable_threshold = DEFAULT_MEMTABLE_THRESHOLD);

    void insert(storage::Key key, const storage::Row& row);

    std::optional<storage::Row> get(storage::Key key) const;

    void scan_begin(); // optional hook later

    void force_freeze(); // manual flush trigger

private:
    void maybe_freeze();

private:
    size_t threshold_;

    std::unique_ptr<MemTable> active_;
    std::vector<std::unique_ptr<ImmutableMemTable>> immutables_;
};

} // namespace htap::lsmtree
