#include "lsmtree/mem/memtable.hpp"
#include "utils/logger.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

void MemTable::insert(Key key, const Row& row) {
    data_[key] = row;

    LOG_DEBUG("MemTable insert key={}", key);
}

size_t MemTable::size() const {
    return data_.size();
}

std::unique_ptr<ImmutableMemTable> MemTable::freeze() {
    std::vector<Row> out;
    out.reserve(data_.size());

    for (auto& [k, v] : data_) {
        out.emplace_back(std::move(v));
    }

    data_.clear();

    LOG_INFO("MemTable freeze: size={}, creating ImmutableMemTable", out.size());

    return std::make_unique<ImmutableMemTable>(std::move(out));
}
