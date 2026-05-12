#pragma once // lsmtree/mem/memtable.hpp

#include <memory>

#include "storage/api/types.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

class IMemTable {
public:
    virtual ~IMemTable() = default;

    virtual void insert(const storage::Row& row) = 0;

    virtual size_t size() const = 0;

    // гарантирует отсортированность данных в ImmMemTable
    virtual std::unique_ptr<ImmutableMemTable> to_sorted_immutable() = 0;
};

} // namespace htap::lsmtree
