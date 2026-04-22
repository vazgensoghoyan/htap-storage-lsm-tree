#pragma once // lsmtree/mem/imm_memtable.hpp

#include <map>
#include <optional>
#include <memory>

#include "storage/api/types.hpp"

namespace htap::lsmtree {

class ImmutableMemTable {
public:
    using Map = std::map<storage::Key, storage::Row>;
    using Iterator = Map::const_iterator;

public:
    explicit ImmutableMemTable(Map&& data);

    std::optional<storage::Row> get(storage::Key key) const;

    Iterator lower_bound(storage::Key key) const;
    Iterator begin() const noexcept;
    Iterator end() const noexcept;

    size_t size() const noexcept;

private:
    Map data_;
};

} // namespace htap::lsmtree
