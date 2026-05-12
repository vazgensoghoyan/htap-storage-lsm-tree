#include "lsmtree/lsmtree.hpp"

#include <filesystem>
#include <stdexcept>
#include <limits>
#include <format>

#include "utils/logger.hpp"
#include "lsmtree/sstable/sstable_builder.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

LSMTree::LSMTree(const Schema& schema, const std::string& path, size_t memtable_threshold)
    : schema_(schema)
    , path_(path)
    , memory_layer_(memtable_threshold)
    , next_sst_id_(0)
{
    std::filesystem::create_directories(path_);

    LOG_INFO("LSMTree initialized at path={}", path_);
}

const Schema& LSMTree::schema() const noexcept {
    return schema_;
}

void LSMTree::insert(const Row& row) {
    memory_layer_.insert(row);
    flush_memtable(); // пока всегда при появлении immutable он сразу будет flush-иться
    // в будущем возможно будем хранить их несколько и только в какой то момент flush-ить
}

void LSMTree::flush_memtable() {
    auto imm = memory_layer_.pop_immutable();
    if (!imm)
        return;

    uint64_t sst_id = next_sst_id_++;
    std::string file_path = build_sst_path(sst_id);

    LOG_INFO("Flushing SSTable id={} to {}", sst_id, file_path);

    SSTableBuilder builder(schema_, file_path);

    for (const auto& row : imm->data()) {
        builder.add(row);
    }

    builder.finish();

    SSTableInfo info{
        .id = sst_id,
        .path = file_path,
        .level = 0, // L0 for now
        .min_key = std::get<Key>(imm->data().front()[KEY_COLUMN_INDEX].value()),
        .max_key = std::get<Key>(imm->data().back()[KEY_COLUMN_INDEX].value()),
        .file_size_bytes = std::filesystem::file_size(file_path),
        .num_blocks = 0, // можно расширить через builder stats
        .layout = SSTLayout::ROW
    };

    registry_.add(info);

    LOG_INFO("SSTable registered id={}, keys belong [{}, {}]", info.id, info.min_key, info.max_key);
}

std::string LSMTree::build_sst_path(uint64_t id) const {
    return std::format("{}/sst_{:020}.sst", path_, id);
}
