#pragma once // lsmtree/mem/imm_memtable.hpp

#include <cstddef>
#include <vector>
#include <optional>

#include "storage/api/types.hpp"

namespace htap::lsmtree {

class ImmutableMemTable {
public:
    using Storage = std::vector<storage::Row>;
    using Iterator = Storage::const_iterator;

    explicit ImmutableMemTable(std::vector<storage::Row>&& data);

    size_t size() const;

    Iterator begin() const;
    Iterator end() const;
    Iterator lower_bound(storage::Key key) const;

private:
    std::vector<storage::Row> data_;
};

} // namespace htap::lsmtree
