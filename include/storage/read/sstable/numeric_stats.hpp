#pragma once

#include "storage/api/types.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <variant>
#include <vector>

namespace htap::storage::read::sstable {

using NumericStatsValue = std::variant<std::int64_t, double>;

struct NumericBlockStats {
    std::size_t column_idx = 0;
    ValueType type = ValueType::INT64;
    bool has_value = false;
    NumericStatsValue min_value = std::int64_t{0};
    NumericStatsValue max_value = std::int64_t{0};
};

struct NumericStatsRange {
    std::uint32_t first_block_id = 0;
    std::uint32_t block_count = 0;
    std::unordered_map<std::size_t, std::vector<NumericBlockStats>> by_column;

    bool empty() const noexcept {
        return by_column.empty();
    }
};

inline bool is_numeric_type(ValueType type) noexcept {
    return type == ValueType::INT64 || type == ValueType::DOUBLE;
}

}
