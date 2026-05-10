#pragma once // storage/mock/mock_cursor.hpp

#include <map>
#include <cstddef>

#include "storage/api/cursor_interface.hpp"

namespace htap::storage {

class MockCursor final : public ICursor {
public:
    MockCursor(const std::map<Key, Row>* data, OptKey from, OptKey to);

    bool valid() const override;
    void next() override;

    Key key() const override;
    NullableValue value(size_t column_idx) const override;

private:
    const Row& row() const;

private:
    using It = std::map<Key, Row>::const_iterator;

private:
    const std::map<Key, Row>* data_;

    It it_;
    It end_;

    OptKey to_;
};

} // namespace htap::storage
