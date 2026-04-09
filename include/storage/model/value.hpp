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

using Value = std::variant<int64_t, double, std::string>;

using NullableValue = std::optional<Value>;

} // namespace htap::storage
