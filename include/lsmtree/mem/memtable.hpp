#pragma once // lsmtree/mem/memtable.hpp

#include <map>
#include <optional>

#include "storage/api/types.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

class MemTable {
public:
    using Map = std::map<storage::Key, storage::Row>;
    using Iterator = Map::const_iterator;

public:
    MemTable() = default;

    void insert(storage::Key key, const storage::Row& row);

    std::optional<storage::Row> get(storage::Key key) const;

    Iterator lower_bound(storage::Key key) const;

    Iterator begin() const noexcept;
    Iterator end() const noexcept;

    size_t size() const noexcept;

    Map&& extract() noexcept;

private:
    Map data_;
};

} // namespace htap::lsmtree
