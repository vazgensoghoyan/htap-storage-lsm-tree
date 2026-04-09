#pragma once // storage/mock/mock_storage_engine.hpp

#include <map>
#include <vector>
#include <memory>

#include "storage/api/storage_engine_interface.hpp"

namespace htap::storage {

class MockStorageEngine final : public IStorageEngine {
public:
    explicit MockStorageEngine(const Schema& schema);

    void insert(
        int64_t key,
        const std::vector<NullableValue>& values) override;

    std::unique_ptr<ICursor> get(
        int64_t key,
        const std::vector<size_t>& projection) const override;

    std::unique_ptr<ICursor> scan(
        int64_t from,
        int64_t to,
        const std::vector<size_t>& projection) const override;

private:
    std::map<int64_t, std::vector<NullableValue>> data_;
};

} // namespace htap::storage
