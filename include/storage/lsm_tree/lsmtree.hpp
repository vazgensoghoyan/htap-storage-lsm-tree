#pragma once

#include <memory>
#include <string>
#include <optional>

#include "memtable.hpp"
#include "row.hpp"
#include "schema.hpp"

namespace htap::storage {

class LSMTree {
public:
    explicit LSMTree(std::shared_ptr<Schema> schema);

    // write path
    void put(const std::string& key, const Row& row);

    // read path
    std::optional<Row> get(const std::string& key) const;

private:
    std::shared_ptr<Schema> schema_;

    std::unique_ptr<MemTable> memtable_;

    // SSTables later
};

} // namespace htap::storage
