#pragma once // storage/model/value.hpp

#include <string>
#include <variant>
#include <cstdint>
#include <optional>
#include <limits>

namespace htap::storage {

enum class ValueType {
    INT64,
    DOUBLE,
    STRING
};

using Key = int64_t;

constexpr Key KEY_MIN = std::numeric_limits<Key>::min();
constexpr Key KEY_MAX = std::numeric_limits<Key>::max();

using Value = std::variant<int64_t, double, std::string>; // either haskell

using NullableValue = std::optional<Value>;

using Row = std::vector<NullableValue>;

} // namespace htap::storage
