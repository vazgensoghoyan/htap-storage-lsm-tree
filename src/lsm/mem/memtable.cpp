#include "lsmtree/mem/memtable.hpp"
#include "lsmtree/mem/memtable_cursor.hpp"
#include "lsmtree/mem/imm_memtable.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

void MemTable::insert(Key key, const Row& row) {
    data_[key] = row;
}

size_t MemTable::size() const noexcept {
    return data_.size();
}

std::unique_ptr<ICursor> MemTable::get(Key key, const std::vector<size_t>& projection) const {
    auto it = data_.find(key);
    if (it == data_.end())
        return nullptr;

    // single-element cursor через scan
    return scan(key, key + 1, projection);
}

std::unique_ptr<ICursor> MemTable::scan(OptKey from,OptKey to, const std::vector<size_t>& projection) const {
    return std::make_unique<MemTableCursor>(
        &data_,
        from,
        to,
        projection
    );
}

std::unique_ptr<ImmutableMemTable> MemTable::freeze() {
    std::vector<Row> out;
    out.reserve(data_.size());

    for (auto& [k, v] : data_) {
        out.emplace_back(std::move(v));
    }

    data_.clear();

    return std::make_unique<ImmutableMemTable>(std::move(out));
}
