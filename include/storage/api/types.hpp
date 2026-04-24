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

using Key = int64_t;
using OptKey = std::optional<Key>;

using Value = std::variant<int64_t, double, std::string>; // either haskell

using NullableValue = std::optional<Value>;

using Row = std::vector<NullableValue>;

} // namespace htap::storage
