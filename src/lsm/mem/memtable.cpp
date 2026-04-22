#include "lsmtree/mem/memtable.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

void MemTable::insert(Key key, const Row& row) {
    data_[key] = row;
}

std::optional<Row> MemTable::get(Key key) const {
    auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;
    return it->second;
}

MemTable::Iterator MemTable::lower_bound(Key key) const {
    return data_.lower_bound(key);
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

MemTable::Map&& MemTable::extract() noexcept {
    return std::move(data_);
}
