#pragma once // lsmtree/mem/memtable.hpp

#include <map>
#include <optional>

#include "storage/api/types.hpp"
#include "storage/model/schema.hpp"

namespace htap::lsmtree {

class MemTable {
public:
    using Map = std::map<storage::Key, storage::Row>;
    using Iterator = Map::const_iterator;

public:
    explicit MemTable(const storage::Schema& schema);

    ~MemTable() = default;

    MemTable(const MemTable&) = delete;
    MemTable& operator=(const MemTable&) = delete;

    MemTable(MemTable&&) = default;
    MemTable& operator=(MemTable&&) = default;

    void insert(storage::Key key, const storage::Row& row);

    std::optional<storage::Row> get(storage::Key key) const;

    Iterator lower_bound(storage::Key key) const;
    Iterator upper_bound(storage::Key key) const;

    Iterator begin() const noexcept;
    Iterator end() const noexcept;

    size_t size() const noexcept;
    bool empty() const noexcept;

private:
    storage::Schema schema_;
    std::map<storage::Key, storage::Row> data_;
};

} // namespace htap::lsmtree
