#pragma once

#include <map>
#include <memory>
#include <optional>

#include "row.hpp"
#include "schema.hpp"

namespace htap::storage {

class MemTable {
public:
    explicit MemTable(std::shared_ptr<Schema> schema);

    // insert or overwrite
    void put(const std::string& key, const Row& row);

    // read
    std::optional<Row> get(const std::string& key) const;

    // range scan support
    std::map<std::string, Row>::const_iterator begin() const;
    std::map<std::string, Row>::const_iterator end() const;

    size_t size() const;

private:
    std::shared_ptr<Schema> schema_;
    std::map<std::string, Row> data_;
};

} // namespace htap::storage
