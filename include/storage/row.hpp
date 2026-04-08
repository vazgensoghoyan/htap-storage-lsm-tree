#pragma once

#include <string>
#include <unordered_map>

#include "value.hpp"

namespace htap::storage {

class Row {
public:
    Row() = default;

    void set_key(const std::string& key);
    const std::string& key() const;

    void set_value(size_t column_index, const Value& value);
    const Value& get_value(size_t column_index) const;

    bool has_value(size_t column_index) const;

private:
    std::string key_;
    std::unordered_map<size_t, Value> values_;
};

} // namespace htap::storage
