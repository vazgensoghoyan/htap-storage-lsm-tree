#pragma once // lsmtree/mem/memtable.hpp

#include <map>
#include <optional>
#include <memory>

#include "storage/api/types.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

class MemTable {
public:
    using Storage = std::map<storage::Key, storage::Row>;
    using Iterator = Storage::const_iterator;

    MemTable() = default;

    void insert(storage::Key key, const storage::Row& row);

    size_t size() const;

    std::unique_ptr<ImmutableMemTable> freeze();

    Iterator begin() const;
    Iterator end() const;
    Iterator lower_bound(storage::Key key) const;

private:
    std::map<storage::Key, storage::Row> data_;
};

} // namespace htap::lsmtree
