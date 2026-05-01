#pragma once // lsmtree/mem/imm_memtable.hpp

#include <map>
#include <optional>

#include "storage/api/types.hpp"

namespace htap::lsmtree {

class ImmutableMemTable {
public:
    explicit ImmutableMemTable(std::vector<storage::Row>&& data);

    size_t size() const;

private:
    std::vector<storage::Row> data_;
};

} // namespace htap::lsmtree
