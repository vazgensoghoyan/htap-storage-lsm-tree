#pragma once

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <cstddef>

#include "storage/model/schema.hpp"
#include "storage/api/types.hpp"
#include "executor/bound_expression.hpp"

namespace htap::executor {

struct BoundStatement {
    virtual ~BoundStatement() = default;
};

struct BoundCreateTableStatement : BoundStatement {
    std::string table_name;
    storage::Schema schema;
};

struct BoundInsertStatement : BoundStatement {
    std::string table_name;
    const storage::Schema* schema = nullptr;
    storage::Row row_values;
};

struct BoundSelectStatement : BoundStatement {
    std::string table_name;
    const storage::Schema* schema = nullptr;
    std::vector<std::unique_ptr<BoundSelectItem>> select_items;
    std::unique_ptr<BoundExpression> where_expression;
    std::vector<std::unique_ptr<BoundExpression>> group_by_items;
    std::vector<BoundOrderByItem> order_by_items;
    std::optional<std::size_t> limit;
};

}