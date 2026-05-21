#pragma once // lsmtree/mem/imm_memtable.hpp

#include <cstddef>
#include <vector>
#include <optional>

#include "storage/api/types.hpp"

namespace htap::lsmtree {

// создается из MemTable::to_sorted_immutable, который вызывается в MemoryLayer
// этим гарантируем отсортированность данных
// строго говоря, этот класс сам ничего не гарантирует, но позже
// когда данные идут в SSTableBuilder, он уже проверяет отсортированность
// поступаемых данных по ключу
class ImmutableMemTable {
public:
    using Storage = std::vector<storage::Row>;
    using Iterator = Storage::const_iterator;

    explicit ImmutableMemTable(std::vector<storage::Row>&& data);

    size_t size() const;
    const std::vector<storage::Row>& data() const;

    Iterator begin() const;
    Iterator end() const;
    Iterator lower_bound(storage::Key key) const;

private:
    std::vector<storage::Row> data_;
};

} // namespace htap::lsmtree
