#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <variant>

#include "storage/api/types.hpp"

namespace htap::executor {

struct CreateTableResult {
};

struct InsertResult {
    std::size_t inserted_rows = 0;
};

struct SelectResult {
    std::vector<std::string> column_names;
    std::vector<storage::Row> rows;
};

using ExecutionResult = std::variant<CreateTableResult, InsertResult, SelectResult>;

}
