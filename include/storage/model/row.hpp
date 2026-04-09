#pragma once // storage/model/row.hpp

#include <vector>
#include <optional>

#include "storage/model/value.hpp"

namespace htap::storage {

/**
 * строка схемы, выровнена по схеме, так как значения хранятся в векторах
 * ключ int64_t хранится отдельно
 */
class Row {
public:
    Row() = default;

    explicit Row(size_t column_count);

    void set_key(int64_t key);
    int64_t key() const;

    void set_value(size_t column_index, const NullableValue& value);
    const NullableValue& get_value(size_t column_index) const;

    bool has_value(size_t column_index) const;

    size_t size() const;

private:
    int64_t key_;
    std::vector<NullableValue> values_;
};

} // namespace htap::storage
