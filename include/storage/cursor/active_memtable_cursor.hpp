#pragma once

#include "storage/api/cursor_interface.hpp"

#include <cstddef>
#include <map>

namespace htap::storage::cursor {

class ActiveMemTableCursor final : public ICursor {
public:
    using Storage = std::map<Key, Row>;
    using Iterator = Storage::const_iterator;

    ActiveMemTableCursor(Iterator begin, Iterator end);

    bool valid() const override;

    void next() override;

    Key key() const override;

    NullableValue value(std::size_t column_idx) const override;

private:
    Iterator current_;
    Iterator end_;
};

}