#pragma once // storage/api/config.hpp

#include <cstddef>
#include <cstdint>
#include <string>

#ifndef HTAP_BACKGROUND_COMPACTION_DEFAULT
#define HTAP_BACKGROUND_COMPACTION_DEFAULT true
#endif

namespace htap::storage {

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

    // Период опроса фонового потока compaction (мс)
    // Поток просыпается по таймауту ИЛИ по сигналу после flush,
    // проверяет политику и при необходимости выполняет compaction
    std::size_t compaction_interval_ms = 100;

    // Режим выполнения flush/compaction:
    //   true  — на отдельном фоновом worker-потоке (insert не блокируется
    //           на время записи SSTable); требует периодического опроса
    //           либо барьера wait_for_compaction для детерминизма;
    //   false — синхронно на потоке insert (старое поведение): flush и
    //           compaction выполняются прямо в insert(), сразу по факту
    //           переполнения memtable. Worker-поток не создаётся.
    // Дефолт задаётся на этапе билда (CMake -DHTAP_BACKGROUND_COMPACTION), но может быть переопределён в рантайме перед созданием хранилища
    bool is_compaction_background = HTAP_BACKGROUND_COMPACTION_DEFAULT;
};

} // namespace htap::storage
