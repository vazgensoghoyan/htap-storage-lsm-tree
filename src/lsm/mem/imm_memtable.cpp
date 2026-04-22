#include "lsmtree/mem/imm_memtable.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

ImmutableMemTable::ImmutableMemTable(Map&& data) : data_(std::move(data)) {}

std::optional<Row> ImmutableMemTable::get(Key key) const {
    auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;
    return it->second;
}

ImmutableMemTable::Iterator ImmutableMemTable::lower_bound(Key key) const {
    return data_.lower_bound(key);
}

ImmutableMemTable::Iterator ImmutableMemTable::begin() const noexcept {
    return data_.begin();
}

ImmutableMemTable::Iterator ImmutableMemTable::end() const noexcept {
    return data_.end();
}

size_t ImmutableMemTable::size() const noexcept {
    return data_.size();
}
