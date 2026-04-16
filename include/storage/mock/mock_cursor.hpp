#pragma once // storage/mock/mock_cursor.hpp

#include <vector>
#include <map>
#include <cstddef>

#include "storage/api/cursor_interface.hpp"

namespace htap::storage {

class MockCursor final : public ICursor {
public:
    MockCursor(
        std::vector<Key> keys,
        const std::map<Key, Row>* data,
        const std::vector<size_t>& projection
    );

    bool valid() const override;
    void next() override;
    void seek(const Key& key) override;

    Key key() const override;
    const NullableValue& value(size_t column_idx) const override;

private:
    void advance_to_valid();
    bool is_projected(size_t column_idx) const;

private:
    std::vector<Key> keys_;
    const std::map<Key, Row>* data_;
    std::vector<size_t> projection_;
    size_t pos_;
};

} // namespace htap::storage
