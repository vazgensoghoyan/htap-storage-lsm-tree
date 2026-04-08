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

using Value = std::variant<
    int64_t,
    double,
    std::string
>;

inline ValueType get_value_type(const Value& v) {
    if (std::holds_alternative<int64_t>(v)) return ValueType::INT64;
    if (std::holds_alternative<double>(v)) return ValueType::DOUBLE;
    return ValueType::STRING;
}

} // namespace htap::storage
