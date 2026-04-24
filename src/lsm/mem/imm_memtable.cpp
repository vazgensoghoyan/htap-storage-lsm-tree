#include "lsmtree/mem/imm_memtable.hpp"
#include "lsmtree/mem/cursors/imm_memtable_cursor.hpp"
#include "utils/logger.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

ImmutableMemTable::ImmutableMemTable(std::vector<Row>&& data) : data_(std::move(data)) {}

size_t ImmutableMemTable::size() const noexcept {
    return data_.size();
}

std::unique_ptr<ICursor> ImmutableMemTable::get(Key key, const std::vector<size_t>& projection) const {
    // linear scan fallback (позже заменю на binary search index)
    size_t idx = 0;

    for (; idx < data_.size(); ++idx) {
        const auto& row = data_[idx];
        Key row_key = std::get<Key>(row[0].value());

        if (row_key == key)
            return std::make_unique<ImmutableMemTableCursor>(
                &data_, idx, idx + 1, projection
            );
    }

    return nullptr;
}

std::unique_ptr<ICursor> ImmutableMemTable::scan(OptKey from, OptKey to, const std::vector<size_t>& projection) const {
    size_t start = 0;
    size_t end = data_.size();

    if (from) {
        while (start < data_.size()) {
            Key k = std::get<Key>(data_[start][0].value());
            if (k >= *from) break;
            ++start;
        }
    }

    if (to) {
        while (end > start) {
            Key k = std::get<Key>(data_[end - 1][0].value());
            if (k < *to) break;
            --end;
        }
    }

    return std::make_unique<ImmutableMemTableCursor>(
        &data_,
        start,
        end,
        projection
    );
}
