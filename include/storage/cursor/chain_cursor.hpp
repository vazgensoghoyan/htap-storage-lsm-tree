#pragma once

#include "storage/api/cursor_interface.hpp"

#include <vector>
#include <memory>
#include <cstddef>

namespace htap::storage::cursor {

class ChainCursor final : public ICursor {
public:
    explicit ChainCursor(std::vector<std::unique_ptr<ICursor>> cursors);

    bool valid() const override;

    void next() override;

    Key key() const override;

    NullableValue value(std::size_t column_idx) const override;

private:
    void skip_invalid_cursors();

private:
    std::vector<std::unique_ptr<ICursor>> cursors_;
    std::size_t current_cursor_ = 0;
};

}