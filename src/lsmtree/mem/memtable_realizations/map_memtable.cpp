#include "lsmtree/mem/memtable_realizations/map_memtable.hpp"
#include "utils/logger.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

void MapMemTable::insert(const Row& row) {
    Key key = std::get<Key>(*row[KEY_COLUMN_INDEX]);
    data_[key] = row;

    LOG_DEBUG("MemTable insert key={}", key);
}

size_t MapMemTable::size() const {
    return data_.size();
}

std::unique_ptr<ImmutableMemTable> MapMemTable::to_sorted_immutable() {
    std::vector<Row> out;
    out.reserve(data_.size());

    for (auto& [k, v] : data_) {
        out.emplace_back(std::move(v));
    }

    data_.clear();

    LOG_INFO("MemTable freeze: size={}, creating ImmutableMemTable", out.size());

    return std::make_unique<ImmutableMemTable>(std::move(out));
}
