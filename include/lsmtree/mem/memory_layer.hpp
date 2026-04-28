#pragma once // lsmtree/mem/memory_layer.hpp

#include <vector>
#include <memory>
#include <optional>

#include "storage/api/types.hpp"
#include "storage/api/cursor_interface.hpp"
#include "lsmtree/interfaces/readable_table_interface.hpp"

#include "lsmtree/mem/memtable.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

namespace htap::lsmtree {

inline constexpr size_t DEFAULT_MEMTABLE_THRESHOLD = 10000;

class MemoryLayer : public IReadableTable {
public:
    MemoryLayer(size_t threshold = DEFAULT_MEMTABLE_THRESHOLD);

    void insert(storage::Key key, const storage::Row& row);

    std::unique_ptr<storage::ICursor> get(
        storage::Key key,
        const std::vector<size_t>& projection) const override;

    std::unique_ptr<storage::ICursor> scan(
        storage::OptKey from,
        storage::OptKey to,
        const std::vector<size_t>& projection) const override;

    void force_freeze();

    size_t immutable_count() const noexcept;

private:
    size_t threshold_;

    std::shared_ptr<MemTable> active_;
    std::vector<std::shared_ptr<ImmutableMemTable>> immutables_;
};

} // namespace htap::lsmtree
