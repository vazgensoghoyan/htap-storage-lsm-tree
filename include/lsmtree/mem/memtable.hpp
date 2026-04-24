#pragma once // lsmtree/mem/memtable.hpp

#include <map>
#include <optional>

#include "storage/api/types.hpp"
#include "lsmtree/mem/imm_memtable.hpp"
#include "lsmtree/interfaces/readable_table_interface.hpp"

namespace htap::lsmtree {

class MemTable : public IReadableTable {
public:
    MemTable() = default;

    void insert(storage::Key key, const storage::Row& row);

    std::unique_ptr<storage::ICursor> get(
        storage::Key key,
        const std::vector<size_t>& projection) const override;

    std::unique_ptr<storage::ICursor> scan(
        storage::OptKey from,
        storage::OptKey to,
        const std::vector<size_t>& projection) const override;

    size_t size() const noexcept;

    std::unique_ptr<ImmutableMemTable> freeze();

private:
    std::map<storage::Key, storage::Row> data_;
};

} // namespace htap::lsmtree
