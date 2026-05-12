#include "lsmtree/lsmtree.hpp"

#include <filesystem>
#include <stdexcept>
#include <limits>
#include <format>

#include "utils/logger.hpp"
#include "lsmtree/sstable/build/sstable_builder.hpp"

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

    SSTableBuildResult build_result = builder.finish();

    // эти две переменные - захардкожено то, куда и в каком виде должно
    // попадать первый sstable при flush-е imm_memtabl-а
    uint32_t level = 0;
    SSTLayout layout = SSTLayout::ROW;

    SSTableInfo info{
        .id = sst_id,
        .path = file_path,
        .level = level,
        .min_key = build_result.min_key,
        .max_key = build_result.max_key,
        .file_size_bytes = std::filesystem::file_size(file_path),
        .meta_offset = build_result.meta_offset,
        .num_blocks = build_result.num_blocks,
        .layout = layout
    };

    registry_.add(info);

    LOG_INFO("SSTable registered id={}, keys belong [{}, {}]", info.id, info.min_key, info.max_key);
}

std::string LSMTree::build_sst_path(uint64_t id) const {
    return std::format("{}/sst_{:020}.sst", path_, id);
}
