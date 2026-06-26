#pragma once

#include "storage/read/data_skipping_filter.hpp"
#include "storage/read/sstable/column_block_meta.hpp"
#include "storage/read/sstable/numeric_stats.hpp"
#include "storage/read/sstable/row_block_meta.hpp"

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace htap::storage::read::sstable {

class DataSkippingBlockPruner {
public:
    std::vector<RowBlockMeta> filter_row_blocks(
        const std::vector<RowBlockMeta>& blocks,
        const NumericStatsRange& stats,
        const read::DataSkippingFilter& filter
    ) const;

    std::vector<std::uint32_t> filter_logical_block_ids(
        const std::vector<std::uint32_t>& block_ids,
        const NumericStatsRange& stats,
        const read::DataSkippingFilter& filter
    ) const;

    std::vector<ColumnBlockMeta> filter_column_blocks(
        const std::vector<ColumnBlockMeta>& blocks,
        const NumericStatsRange& stats,
        const read::DataSkippingFilter& filter
    ) const;

private:
    bool may_satisfy(
        std::uint32_t block_id,
        const NumericStatsRange& stats,
        const read::DataSkippingFilter& filter
    ) const;
};

}
