#pragma once

#include "storage/api/cursor_interface.hpp"

#include <cstddef>
#include <vector>

namespace htap::storage::cursor {

class ImmutableMemTableCursor final : public ICursor {
public:
    using Storage = std::vector<Row>;
    using Iterator = Storage::const_iterator;

    ImmutableMemTableCursor(Iterator begin, Iterator end);

    bool valid() const override;

    void next() override;

    Key key() const override;

    NullableValue value(std::size_t column_idx) const override;

private:
    Iterator current_;
    Iterator end_;
};

}