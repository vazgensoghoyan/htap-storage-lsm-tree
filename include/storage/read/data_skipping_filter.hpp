#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace htap::storage::read {

enum class NumericComparisonOp {
    Equal,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
};

using NumericComparisonOperation = NumericComparisonOp;

using NumericPredicateValue = std::variant<std::int64_t, double>;

struct NumericColumnPredicate {
    std::size_t column_idx;
    NumericComparisonOp op;
    NumericPredicateValue value;
};

struct DataSkippingFilter {

    std::vector<NumericColumnPredicate> predicates;

    bool empty() const noexcept {
        return predicates.empty();
    }

    std::vector<std::size_t> referenced_columns() const {

        std::vector<std::size_t> columns;
        columns.reserve(predicates.size());

        for (const auto& predicate : predicates) {
            columns.push_back(predicate.column_idx);
        }

        std::sort(columns.begin(), columns.end());
        columns.erase(std::unique(columns.begin(), columns.end()), columns.end());

        return columns;
        
    }
};

} 
