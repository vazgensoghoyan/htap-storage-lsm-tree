#pragma once

#include <string>
#include <vector>
#include <stdexcept>

#include "value.hpp"

namespace htap::storage {

struct Column {
    std::string name;
    ValueType type;
    bool is_key = false;
};

class Schema {
public:
    Schema() = default;

    void add_column(const std::string& name, ValueType type, bool is_key = false);

    const std::vector<Column>& columns() const;

    size_t size() const;

    int get_column_index(const std::string& name) const;

    const Column& get_column(size_t index) const;

    bool is_valid_value(size_t column_index, const Value& value) const;

    int key_column_index() const;

private:
    std::vector<Column> columns_;
    int key_index_ = -1;
};

} // namespace htap::storage
