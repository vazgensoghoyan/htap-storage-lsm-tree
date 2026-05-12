#pragma once // lsmtree/mem/imm_memtable.hpp

#include <map>
#include <optional>

#include "storage/api/types.hpp"

namespace htap::lsmtree {

// считаем, что на вход всегда сортированные данные приходят (по ключу)

class ImmutableMemTable {
public:
    explicit ImmutableMemTable(std::vector<storage::Row>&& data);

    size_t size() const;
    const std::vector<storage::Row>& data() const;

private:
    std::vector<storage::Row> data_;
};

} // namespace htap::lsmtree
