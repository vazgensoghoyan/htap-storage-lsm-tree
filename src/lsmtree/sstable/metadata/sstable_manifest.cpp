#include "lsmtree/sstable/metadata/sstable_manifest.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "lsmtree/sstable/format/sst_layout.hpp"
#include "utils/binary_writer.hpp"
#include "utils/logger.hpp"

using namespace htap::lsmtree::sstable;
using namespace htap::storage;

namespace {

template<typename T>
T read_pod(std::istream& in, const std::filesystem::path& path) {
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!in || in.gcount() != static_cast<std::streamsize>(sizeof(T)))
        throw std::runtime_error("Manifest: unexpected end of file: " + path.string());
    return value;
}

std::string read_string(std::istream& in, uint16_t len, const std::filesystem::path& path) {
    std::string s(len, '\0');
    in.read(s.data(), len);
    if (!in || in.gcount() != static_cast<std::streamsize>(len))
        throw std::runtime_error("Manifest: failed to read path string: " + path.string());
    return s;
}

} // namespace

std::filesystem::path SSTableManifest::manifest_path(const std::filesystem::path& dir) {
    return dir / "manifest.bin";
}

std::filesystem::path SSTableManifest::manifest_tmp_path(const std::filesystem::path& dir) {
    return dir / "manifest.tmp";
}

void SSTableManifest::save(
    const std::filesystem::path& path,
    const SSTableRegistry& registry,
    uint64_t next_sst_id
) {
    const auto tmp = manifest_tmp_path(path);
    const auto dst = manifest_path(path);

    // Собираем все SST через все уровни
    std::vector<SSTableInfo> all_entries;
    for (size_t level = 0; level < registry.level_count(); ++level) {
        const auto& level_sstables = registry.sstables_at_level(
            static_cast<uint32_t>(level)
        );
        for (const auto& info : level_sstables) {
            all_entries.push_back(info);
        }
    }

    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        throw std::runtime_error("Manifest: failed to open for writing: " + tmp.string());

    utils::BinaryWriter w(out);

    // Header
    w.write_u32(MAGIC);
    w.write_u32(static_cast<uint32_t>(all_entries.size()));
    w.write_u64(next_sst_id);

    // Entries
    for (const auto& info : all_entries) {
        const uint16_t path_len = static_cast<uint16_t>(
            std::min(info.path.size(), static_cast<size_t>(65535u))
        );

        w.write_u64(info.id);
        w.write_u32(info.level);
        w.write_u8(static_cast<uint8_t>(info.layout));
        w.write_i64(info.min_key);
        w.write_i64(info.max_key);
        w.write_u32(info.num_blocks);
        w.write_u64(info.file_size_bytes);
        w.write_u16(path_len);
        w.write_bytes(info.path.data(), path_len);
    }

    out.flush();
    out.close();

    // Атомарная замена
    std::filesystem::rename(tmp, dst);

    LOG_INFO("Manifest saved: {} entries, next_sst_id={}", all_entries.size(), next_sst_id);
}

void SSTableManifest::load(
    const std::filesystem::path& path,
    SSTableRegistry& registry,
    uint64_t& next_sst_id
) {
    const auto manifest = manifest_path(path);

    if (!std::filesystem::exists(manifest)) {
        next_sst_id = 0;
        LOG_INFO("Manifest not found at {}, starting fresh", manifest.string());
        return;
    }

    std::ifstream in(manifest, std::ios::binary);
    if (!in.is_open())
        throw std::runtime_error("Manifest: failed to open for reading: " + manifest.string());

    // Header
    const uint32_t magic = read_pod<uint32_t>(in, manifest);
    if (magic != MAGIC)
        throw std::runtime_error("Manifest: invalid magic in " + manifest.string());

    const uint32_t entry_count = read_pod<uint32_t>(in, manifest);
    next_sst_id = read_pod<uint64_t>(in, manifest);

    // Entries
    for (uint32_t i = 0; i < entry_count; ++i) {
        SSTableInfo info;

        info.id              = read_pod<uint64_t>(in, manifest);
        info.level           = read_pod<uint32_t>(in, manifest);
        const uint8_t layout = read_pod<uint8_t>(in, manifest);
        info.layout          = static_cast<SSTLayout>(layout);
        info.min_key         = read_pod<Key>(in, manifest);
        info.max_key         = read_pod<Key>(in, manifest);
        info.num_blocks      = read_pod<uint32_t>(in, manifest);
        info.file_size_bytes = read_pod<uint64_t>(in, manifest);
        const uint16_t path_len = read_pod<uint16_t>(in, manifest);
        info.path            = read_string(in, path_len, manifest);

        registry.add(info);
    }

    LOG_INFO("Manifest loaded: {} entries, next_sst_id={}", entry_count, next_sst_id);
}
