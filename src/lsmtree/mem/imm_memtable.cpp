#include "lsmtree/mem/imm_memtable.hpp"
#include "utils/logger.hpp"

#include <algorithm>
#include <stdexcept>
#include <variant>

using namespace htap::lsmtree;
using namespace htap::storage;

ImmutableMemTable::ImmutableMemTable(std::vector<Row>&& data) : data_(std::move(data)) {}

size_t ImmutableMemTable::size() const {
    return data_.size();
}

ImmutableMemTable::Iterator ImmutableMemTable::begin() const {
    return data_.begin();
}

ImmutableMemTable::Iterator ImmutableMemTable::end() const {
    return data_.end();
}

ImmutableMemTable::Iterator ImmutableMemTable::lower_bound(storage::Key key) const {
    return std::lower_bound(
        data_.begin(),
        data_.end(),
        key,
        [](const storage::Row& row, storage::Key target) {
            return std::get<Key>(*row[KEY_COLUMN_INDEX]) < target;
        }
    );
const std::vector<Row>& ImmutableMemTable::data() const {
    return data_;
}
