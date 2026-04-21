#include "lsmtree/mem/memtable.hpp"

#include <stdexcept>

using namespace htap::lsmtree;
using namespace htap::storage;

MemTable::MemTable(const Schema& schema) : schema_(schema) {}

void MemTable::insert(Key key, const Row& row) {
    if (row.size() != schema_.size())
        throw std::runtime_error("MemTable: row size mismatch schema");

    for (size_t i = 0; i < row.size(); ++i) {
        if (!schema_.is_valid_value(i, row[i]))
            throw std::runtime_error("MemTable: invalid value for column");
    }

    data_[key] = std::move(row);
}

std::optional<Row> MemTable::get(Key key) const {
    auto it = data_.find(key);
    if (it == data_.end())
        return std::nullopt;
    return it->second;
}

MemTable::Iterator MemTable::lower_bound(Key key) const {
    return data_.lower_bound(key);
}

MemTable::Iterator MemTable::upper_bound(Key key) const {
    return data_.upper_bound(key);
}

MemTable::Iterator MemTable::begin() const noexcept {
    return data_.begin();
}

MemTable::Iterator MemTable::end() const noexcept {
    return data_.end();
}

size_t MemTable::size() const noexcept {
    return data_.size();
}

bool MemTable::empty() const noexcept {
    return data_.empty();
}
