#pragma once // lsmtree/mem/imm_memtable.hpp

#include <map>
#include <optional>

#include "storage/api/types.hpp"
#include "lsmtree/mem/memtable.hpp"

namespace htap::lsmtree {

class ImmutableMemTable {
public:
    using Map = std::map<storage::Key, storage::Row>;
    using Iterator = Map::const_iterator;

public:
    explicit ImmutableMemTable(Map&& mem);

    std::optional<storage::Row> get(storage::Key key) const;

    Iterator lower_bound(storage::Key key) const;

    Iterator begin() const noexcept;
    Iterator end() const noexcept;

    size_t size() const noexcept;

private:
    Map data_;
};

} // namespace htap::lsmtree
