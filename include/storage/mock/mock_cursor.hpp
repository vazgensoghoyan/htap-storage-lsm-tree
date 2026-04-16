#pragma once // storage/mock/mock_cursor.hpp

#include <map>
#include <vector>
#include <cstddef>
#include <memory>

#include "storage/api/cursor_interface.hpp"
#include "storage/model/value.hpp"

namespace htap::storage {

class MockCursor : public ICursor {
public:
    using MockStorage = std::map<Key, Row>;
    using MockIterator = MockStorage::const_iterator;

public:
    MockCursor(
        const MockStorage& data,
        std::vector<size_t> projection,
        Key from,
        Key to);

    ~MockCursor() override = default;

    bool valid() const override;

    void next() override;

    void seek(const Key& key) override;

    Key key() const override;

    bool is_null(size_t column_idx) const override;

    const Value& value(size_t column_idx) const override;

private:
    // Проверка, что колонка есть в projection
    void check_projection(size_t column_idx) const;

private:
    const MockStorage& data_;

    MockIterator it_;
    MockIterator end_;

    std::vector<size_t> projection_;
};

} // namespace htap::storage
