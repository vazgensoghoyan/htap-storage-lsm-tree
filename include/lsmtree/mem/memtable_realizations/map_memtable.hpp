#pragma once // lsmtree/mem/memtable_realizations/memtable.hpp

#include <map>
#include <memory>

#include "storage/api/types.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

#include "lsmtree/mem/memtable_interface.hpp"

namespace htap::lsmtree {

class MapMemTable : public IMemTable {
public:
    MapMemTable() = default;

    void insert(const storage::Row& row) override;

    size_t size() const override;

    std::unique_ptr<ImmutableMemTable> to_sorted_immutable() override;

private:
    std::map<storage::Key, storage::Row> data_;
};

} // namespace htap::lsmtree
