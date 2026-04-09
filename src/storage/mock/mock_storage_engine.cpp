#include "storage/mock/mock_storage_engine.hpp"
#include "storage/mock/mock_cursor.hpp"

#include <stdexcept>

using namespace htap::storage;

MockStorageEngine::MockStorageEngine(const Schema& schema) : IStorageEngine(std::move(schema))
{}

void MockStorageEngine::insert(
    int64_t key,
    const std::vector<NullableValue>& values)
{
    if (values.size() != schema_.size()) {
        throw std::invalid_argument("Values size does not match schema");
    }

    for (size_t i = 0; i < values.size(); ++i) {
        if (!schema_.is_valid_value(i, values[i])) {
            throw std::invalid_argument("Invalid value for column: " + schema_.get_column(i).name);
        }
    }

    if (data_.contains(key)) {
        throw std::runtime_error("Duplicate key");
    }

    data_[key] = values;
}

std::unique_ptr<ICursor> MockStorageEngine::get(
    int64_t key,
    const std::vector<size_t>& projection) const
{
    auto it = data_.find(key);
    if (it == data_.end()) {
        // empty cursor
        return std::make_unique<MockCursor>(
            std::vector<int64_t>{},
            std::vector<std::vector<NullableValue>>{},
            projection
        );
    }

    return std::make_unique<MockCursor>(
        std::vector<int64_t> { key },
        std::vector<std::vector<NullableValue>> { it->second },
        projection
    );
}

std::unique_ptr<ICursor> MockStorageEngine::scan(
    int64_t from,
    int64_t to,
    const std::vector<size_t>& projection) const
{
    if (from > to) throw ""; // TODO

    std::vector<int64_t> keys;
    std::vector<std::vector<NullableValue>> rows;

    for (auto it = data_.lower_bound(from); it != data_.end() && it->first <= to; ++it) {
        keys.push_back(it->first);
        rows.push_back(it->second);
    }

    return std::make_unique<MockCursor>(
        std::move(keys),
        std::move(rows),
        projection
    );
}
