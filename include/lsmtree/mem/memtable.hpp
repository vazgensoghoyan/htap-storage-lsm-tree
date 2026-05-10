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

    void insert(storage::Key key, const storage::Row& row);

    size_t size() const;

    std::unique_ptr<ImmutableMemTable> freeze();

private:
    std::map<storage::Key, storage::Row> data_;
};

} // namespace htap::lsmtree
