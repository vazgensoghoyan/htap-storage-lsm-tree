#pragma once

#include "storage/api/cursor_interface.hpp"

namespace htap::storage::cursor {

class EmptyCursor final : public ICursor {
public:
    EmptyCursor() = default;

    bool valid() const override;

    void next() override;

    Key key() const override;

    NullableValue value(std::size_t column_idx) const override;
};

}