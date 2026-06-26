#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "lsmtree/sstable/format/sparse_index_entry.hpp"

namespace htap::lsmtree::sstable {

struct SparseIndexOptions {
    // 0 means adaptive. Non-zero keeps the old fixed-step behaviour.
    std::uint32_t fixed_step = 0;

    // Target sparse.idx budget per SSTable. Adaptive step grows when block count grows.
    std::size_t target_index_bytes = 1024 * 1024;

    std::uint32_t min_step = 1;
    std::uint32_t max_step = 4096;
};

inline std::uint32_t choose_sparse_index_step(
    std::uint32_t num_blocks,
    const SparseIndexOptions& options
) noexcept {
    if (num_blocks == 0) {
        return 1;
    }

    const auto min_step = std::max<std::uint32_t>(1, options.min_step);
    const auto max_step = std::max(min_step, options.max_step);

    if (options.fixed_step != 0) {
        return std::clamp(options.fixed_step, min_step, max_step);
    }

    if (options.target_index_bytes < SPARSE_INDEX_ENTRY_ON_DISK_SIZE) {
        return max_step;
    }

    const auto max_entries = std::max<std::size_t>(
        1,
        options.target_index_bytes / SPARSE_INDEX_ENTRY_ON_DISK_SIZE
    );

    const auto raw_step =
        (static_cast<std::size_t>(num_blocks) + max_entries - 1) / max_entries;

    return std::clamp(
        static_cast<std::uint32_t>(std::min<std::size_t>(
            raw_step,
            std::numeric_limits<std::uint32_t>::max()
        )),
        min_step,
        max_step
    );
}

} // namespace htap::lsmtree::sstable
