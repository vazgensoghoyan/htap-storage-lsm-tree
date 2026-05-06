#pragma once // storage/api/types.hpp

#include <string>
#include <variant>
#include <cstdint>
#include <optional>
#include <limits>
#include <vector>

namespace htap::storage {

enum class ValueType {
    INT64,
    DOUBLE,
    STRING
};

enum class ScanOrder {
    Unordered,
    KeyAscending
};

using Key = int64_t;
using OptKey = std::optional<Key>;

using Value = std::variant<int64_t, double, std::string>;

using NullableValue = std::optional<Value>;

using Row = std::vector<NullableValue>;

inline constexpr size_t KEY_COLUMN_INDEX = 0;

} // namespace htap::storage
