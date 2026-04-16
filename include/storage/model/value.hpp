#pragma once // storage/model/value.hpp

#include <string>
#include <variant>
#include <cstdint>

namespace htap::storage {

enum class ValueType {
    INT64,
    DOUBLE,
    STRING
};

using Key = int64_t;

using Value = std::variant<int64_t, double, std::string>; // either haskell

using NullableValue = std::optional<Value>;

} // namespace htap::storage
