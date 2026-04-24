#pragma once // lsmtree/mem/cursors/memtable_cursor.hpp

#include <map>
#include <vector>
#include <optional>
#include <cstddef>

#include "storage/api/cursor_interface.hpp"
#include "storage/api/types.hpp"

namespace htap::lsmtree {

class MemTableCursor final : public storage::ICursor {
public:
    using Map = std::map<storage::Key, storage::Row>;

public:
    MemTableCursor(
        const Map* data,
        storage::OptKey from,
        storage::OptKey to,
        std::vector<size_t> projection
    );

    bool valid() const override;
    void next() override;

    storage::Key key() const override;
    storage::NullableValue value(size_t column_idx) const override;
    const storage::Row& row() const override;

private:
    bool is_projected(size_t column_idx) const;

private:
    const Map* data_;

    Map::const_iterator it_;
    Map::const_iterator end_;

    storage::OptKey to_;

    std::vector<size_t> projection_;
    const storage::Row* current_row_ = nullptr;
};

} // namespace htap::lsmtree
