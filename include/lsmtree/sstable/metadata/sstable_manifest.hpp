#pragma once // lsmtree/sstable/metadata/sstable_manifest.hpp

#include <cstdint>
#include <filesystem>

#include "lsmtree/sstable/metadata/sstable_registry.hpp"

namespace htap::lsmtree::sstable {

/**
 * Персистентность реестра SSTable.
 *
 * Формат файла manifest.bin:
 *
 * Header:
 *   uint32  magic       = 0x48544150 ("HTAP")
 *   uint32  entry_count
 *   uint64  next_sst_id  // следующий свободный id для новых SST
 *
 * Entry (entry_count записей):
 *   uint64  id
 *   uint32  level
 *   uint8   layout        // SSTLayout: 0=ROW, 1=COLUMN
 *   int64   min_key
 *   int64   max_key
 *   uint32  num_blocks
 *   uint64  file_size_bytes
 *   uint16  path_len
 *   char[]  path           // path_len байт, без нулевого терминатора
 *
 * При compaction/flush пишется manifest.tmp, затем переименовывается
 * в manifest.bin (атомарно на POSIX).
 */
class SSTableManifest {
public:
    static constexpr uint32_t MAGIC   = 0x48544150u; // "HTAP"

    // Сохраняет реестр в файл
    static void save(
        const std::filesystem::path& path,
        const SSTableRegistry& registry,
        uint64_t next_sst_id
    );

    // Загружает реестр из файла
    // Если файл не существует — возвращает пустой реестр и next_sst_id=0
    static void load(
        const std::filesystem::path& path,
        SSTableRegistry& registry,
        uint64_t& next_sst_id
    );

private:
    static std::filesystem::path manifest_path(const std::filesystem::path& dir);
    static std::filesystem::path manifest_tmp_path(const std::filesystem::path& dir);
};

} // namespace htap::lsmtree::sstable
