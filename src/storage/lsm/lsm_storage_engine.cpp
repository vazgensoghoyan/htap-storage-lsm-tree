#include "storage/lsm/lsm_storage_engine.hpp"

#include <filesystem>
#include <limits>
#include <stdexcept>
#include <utility>

#include "storage/read/sstable/key_range.hpp"

using namespace htap::storage;

namespace {

void validate_projection(
    const Schema& schema,
    const std::vector<std::size_t>& projection
) {
    std::vector<bool> used(schema.size(), false);

    for (std::size_t column_idx : projection) {
        if (column_idx >= schema.size()) {
            throw std::runtime_error("Projection column index out of schema range");
        }

        if (used[column_idx]) {
            throw std::runtime_error("Projection contains duplicate column index");
        }

        used[column_idx] = true;
    }
}

void validate_row(const Schema& schema, const Row& values) {
    if (values.size() != schema.size()) {
        throw std::runtime_error("Row size mismatch schema");
    }

    for (std::size_t column_idx = 0; column_idx < values.size(); ++column_idx) {
        if (!schema.is_valid_value(column_idx, values[column_idx])) {
            throw std::runtime_error("Invalid value for column");
        }
    }

    const auto& key_cell = values[KEY_COLUMN_INDEX];

    if (!key_cell.has_value()) {
        throw std::runtime_error("Key cannot be NULL");
    }

}

} // namespace

LSMStorageEngine::LSMStorageEngine(StorageConfig config) : config_(std::move(config))
{
    std::filesystem::create_directories(config_.root_path);
}

void LSMStorageEngine::create_table(
    const std::string& table_name,
    const Schema& schema
) {
    if (tables_.contains(table_name)) {
        throw std::runtime_error("Table already exists");
    }

    const std::string path = table_path(table_name);

    StorageConfig table_config = config_;
    table_config.root_path = path;

    tables_.emplace(
        table_name,
        std::make_unique<htap::lsmtree::LSMTree>(schema, table_config)
    );
}

bool LSMStorageEngine::table_exists(const std::string& table_name) const {
    return tables_.contains(table_name);
}

const Schema& LSMStorageEngine::get_table_schema(const std::string& table_name) const {
    return get_tree(table_name).schema();
}

void LSMStorageEngine::insert(
    const std::string& table_name,
    const Row& values
) {
    auto& tree = get_tree(table_name);

    validate_row(tree.schema(), values);

    tree.insert(values);
}

std::unique_ptr<ICursor> LSMStorageEngine::get(
    const std::string& table_name,
    Key key,
    const std::vector<std::size_t>& projection
) const {
    const auto& tree = get_tree(table_name);

    validate_projection(tree.schema(), projection);

    OptKey to = std::nullopt;

    if (key != std::numeric_limits<Key>::max()) {
        to = key + 1;
    }

    read::sstable::KeyRange range{
        .from = key,
        .to = to
    };

    return tree.scan(
        range,
        projection,
        ScanOrder::Unordered
    );
}

std::unique_ptr<ICursor> LSMStorageEngine::scan(
    const std::string& table_name,
    std::optional<Key> from,
    std::optional<Key> to,
    const std::vector<std::size_t>& projection,
    ScanOrder order,
    const read::DataSkippingFilter& data_skipping_filter
) const {
    const auto& tree = get_tree(table_name);

    validate_projection(tree.schema(), projection);

    read::sstable::KeyRange range{
        .from = from,
        .to = to
    };

    return tree.scan(
        range,
        projection,
        order,
        data_skipping_filter
    );
}

void LSMStorageEngine::wait_for_compaction(const std::string& table_name) {
    get_tree(table_name).wait_for_compaction();
}

htap::lsmtree::LSMTree& LSMStorageEngine::get_tree(const std::string& table_name) {
    auto it = tables_.find(table_name);

    if (it == tables_.end()) {
        throw std::runtime_error("Table does not exist");
    }

    return *it->second;
}

const htap::lsmtree::LSMTree& LSMStorageEngine::get_tree(const std::string& table_name) const {
    auto it = tables_.find(table_name);

    if (it == tables_.end()) {
        throw std::runtime_error("Table does not exist");
    }

    return *it->second;
}

std::string LSMStorageEngine::table_path(const std::string& table_name) const {
    return (std::filesystem::path(config_.root_path) / table_name).string();
}
