#pragma once // lsmtree/mem/memtable.hpp

#include <map>
#include <optional>
#include <memory>

#include "storage/api/types.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

class MemTable {
public:
    MemTable() = default;

    void insert(const storage::Row& row);

    size_t size() const;

    // гарантирует отсортированность данных в ImmMemTable
    std::unique_ptr<ImmutableMemTable> to_sorted_immutable();

private:
    std::map<storage::Key, storage::Row> data_;
};

} // namespace htap::lsmtree
