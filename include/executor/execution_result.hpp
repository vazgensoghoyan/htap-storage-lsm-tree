#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <variant>

namespace htap::executor {

using ResultValue = std::variant<std::int64_t, double, std::string, bool>;
using NullableResultValue = std::optional<ResultValue>;
using ResultRow = std::vector<NullableResultValue>;

struct CreateTableResult {
};

struct InsertResult {
    std::size_t inserted_rows = 0;
};

struct SelectResult {
    std::vector<std::string> column_names;
    std::vector<ResultRow> rows;
};

using ExecutionResult = std::variant<CreateTableResult, InsertResult, SelectResult>;

}
