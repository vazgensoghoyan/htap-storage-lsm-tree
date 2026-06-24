#pragma once // lsmtree/config.hpp

#include <cstddef>
#include <cstdint>
#include <string>

namespace htap::lsmtree {

struct StorageConfig {
    // Корневой путь хранилища (директория под все таблицы)
    std::string root_path = "./htap_data";

    // Порог memtable по числу строк; при достижении — freeze + flush
    std::size_t memtable_threshold = 10'000;

    // Максимальное число SSTable на Level-0 до триггера compaction
    // При достижении — все SST с L0 компактируются в L1
    std::size_t level0_compaction_trigger = 4;

    // Размерный коэффициент между соседними уровнями (T в LASER/LSMT)
    // Level-i вмещает size_ratio^i * base_level_size_bytes байт
    std::size_t size_ratio = 10;

    // Базовый размер Level-1 в байтах (Level-0 ограничен числом SST)
    uint64_t base_level_size_bytes = 256ULL * 1024 * 1024; // 256 MB

    // Начиная с какого уровня использовать COLUMN layout
    // Уровни [0, row_to_column_level) → ROW
    // Уровни [row_to_column_level, ∞) → COLUMN
    uint32_t row_to_column_level = 2;

    // Шаг sparse index: запись в sparse.idx каждые N logical blocks
    uint32_t sparse_index_step = 1000;
};

} // namespace htap::lsmtree
