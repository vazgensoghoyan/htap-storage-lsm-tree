#pragma once // storage/mock/mock_cursor.hpp

#include <vector>
#include <memory>

#include "storage/api/cursor_interface.hpp"

namespace htap::storage {

class MockCursor final : public ICursor {
public:
    MockCursor(
        std::vector<int64_t> keys,
        std::vector<std::vector<NullableValue>> rows,
        std::vector<size_t> projection);

    bool valid() const override;
    void next() override;

    int64_t key() const override;
    bool is_null(size_t column_idx) const override;
    const Value& value(size_t column_idx) const override;

private:
    std::vector<int64_t> keys_;
    std::vector<std::vector<NullableValue>> rows_;
    std::vector<size_t> projection_;

    size_t index_ = 0;
};

} // namespace htap::storage
