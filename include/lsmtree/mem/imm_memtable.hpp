#pragma once // lsmtree/mem/imm_memtable.hpp

#include <map>
#include <optional>

#include "storage/api/types.hpp"
#include "lsmtree/interfaces/readable_table_interface.hpp"

namespace htap::lsmtree {

class ImmutableMemTable : public IReadableTable {
public:
    explicit ImmutableMemTable(std::vector<storage::Row>&& data);

    std::unique_ptr<storage::ICursor> get(
        storage::Key key,
        const std::vector<size_t>& projection) const override;

    std::unique_ptr<storage::ICursor> scan(
        storage::OptKey from,
        storage::OptKey to,
        const std::vector<size_t>& projection) const override;

    size_t size() const noexcept;

private:
    std::vector<storage::Row> data_;
};

} // namespace htap::lsmtree
