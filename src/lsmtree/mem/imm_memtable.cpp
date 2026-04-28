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
    auto it = std::lower_bound(
        data_.begin(),
        data_.end(),
        key,
        [](const Row& row, const Key& k) {
            return std::get<Key>(row[KEY_COLUMN_INDEX].value()) < k;
        }
    );

    if (it == data_.end() ||
        std::get<Key>((*it)[KEY_COLUMN_INDEX].value()) != key)
        return nullptr;

    size_t idx = std::distance(data_.begin(), it);

    return std::make_unique<ImmutableMemTableCursor>(
        &data_, idx, idx + 1, projection
    );
}

std::unique_ptr<ICursor> ImmutableMemTable::scan(OptKey from, OptKey to, const std::vector<size_t>& projection) const {
    auto comp = [](const storage::Row& row, const storage::Key& k) {
        return std::get<storage::Key>(row[KEY_COLUMN_INDEX].value()) < k;
    };

    auto start_it = from
        ? std::lower_bound(data_.begin(), data_.end(), *from, comp)
        : data_.begin();

    auto end_it = to
        ? std::lower_bound(data_.begin(), data_.end(), *to, comp)
        : data_.end();

    size_t start = std::distance(data_.begin(), start_it);
    size_t end   = std::distance(data_.begin(), end_it);

    return std::make_unique<ImmutableMemTableCursor>(
        &data_,
        start,
        end,
        projection
    );
}
