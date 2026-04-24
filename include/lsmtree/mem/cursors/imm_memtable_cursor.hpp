#pragma once // lsmtree/mem/cursors/imm_memtable_cursor.hpp

#include <vector>
#include <optional>
#include <cstddef>

#include "storage/api/cursor_interface.hpp"
#include "storage/api/types.hpp"

namespace htap::lsmtree {

class ImmutableMemTableCursor final : public storage::ICursor {
public:
    ImmutableMemTableCursor(
        const std::vector<storage::Row>* data,
        size_t start,
        size_t end,
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
    const std::vector<storage::Row>* data_;

    size_t idx_;
    size_t end_;

    std::vector<size_t> projection_;

    const storage::Row* current_ = nullptr;
};

} // namespace htap::lsmtree
