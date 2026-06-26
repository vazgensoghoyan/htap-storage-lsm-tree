#include "storage/read/sstable/data_skipping_block_pruner.hpp"

#include <algorithm>
#include <limits>

namespace htap::storage::read::sstable {

namespace {

double numeric_value_as_double(const NumericStatsValue& value) {
    if (const auto* int_value = std::get_if<std::int64_t>(&value)) {
        return static_cast<double>(*int_value);
    }

    return std::get<double>(value);
}

bool block_may_satisfy_predicate(
    const NumericBlockStats& stats,
    const read::NumericColumnPredicate& predicate
) {
    if (!stats.has_value) {
        return false;
    }

    if (stats.type == ValueType::INT64 && std::holds_alternative<std::int64_t>(predicate.value)) {
        const auto block_min = std::get<std::int64_t>(stats.min_value);
        const auto block_max = std::get<std::int64_t>(stats.max_value);
        const auto value = std::get<std::int64_t>(predicate.value);

        switch (predicate.op) {
            case read::NumericComparisonOp::Equal:
                return !(value < block_min || value > block_max);
            case read::NumericComparisonOp::Greater:
                return block_max > value;
            case read::NumericComparisonOp::GreaterEqual:
                return block_max >= value;
            case read::NumericComparisonOp::Less:
                return block_min < value;
            case read::NumericComparisonOp::LessEqual:
                return block_min <= value;
        }

        return true;
    }

    const auto block_min = numeric_value_as_double(stats.min_value);
    const auto block_max = numeric_value_as_double(stats.max_value);
    const auto value = numeric_value_as_double(predicate.value);

    switch (predicate.op) {
        case read::NumericComparisonOp::Equal:
            return !(value < block_min || value > block_max);
        case read::NumericComparisonOp::Greater:
            return block_max > value;
        case read::NumericComparisonOp::GreaterEqual:
            return block_max >= value;
        case read::NumericComparisonOp::Less:
            return block_min < value;
        case read::NumericComparisonOp::LessEqual:
            return block_min <= value;
    }

    return true;
}

}

std::vector<RowBlockMeta> DataSkippingBlockPruner::filter_row_blocks(
    const std::vector<RowBlockMeta>& blocks,
    const NumericStatsRange& stats,
    const read::DataSkippingFilter& filter
) const {
    if (blocks.empty() || stats.empty() || filter.empty()) {
        return blocks;
    }

    std::vector<RowBlockMeta> selected;
    selected.reserve(blocks.size());

    for (const auto& block : blocks) {
        if (may_satisfy(static_cast<std::uint32_t>(block.block_id), stats, filter)) {
            selected.push_back(block);
        }
    }

    return selected;
}

std::vector<std::uint32_t> DataSkippingBlockPruner::filter_logical_block_ids(
    const std::vector<std::uint32_t>& block_ids,
    const NumericStatsRange& stats,
    const read::DataSkippingFilter& filter
) const {
    if (block_ids.empty() || stats.empty() || filter.empty()) {
        return block_ids;
    }

    std::vector<std::uint32_t> selected;
    selected.reserve(block_ids.size());

    for (std::uint32_t block_id : block_ids) {
        if (may_satisfy(block_id, stats, filter)) {
            selected.push_back(block_id);
        }
    }

    return selected;
}

std::vector<ColumnBlockMeta> DataSkippingBlockPruner::filter_column_blocks(
    const std::vector<ColumnBlockMeta>& blocks,
    const NumericStatsRange& stats,
    const read::DataSkippingFilter& filter
) const {
    if (blocks.empty() || stats.empty() || filter.empty()) {
        return blocks;
    }

    std::vector<ColumnBlockMeta> selected;
    selected.reserve(blocks.size());

    for (const auto& block : blocks) {
        if (may_satisfy(static_cast<std::uint32_t>(block.block_id), stats, filter)) {
            selected.push_back(block);
        }
    }

    return selected;
}

bool DataSkippingBlockPruner::may_satisfy(
    std::uint32_t block_id,
    const NumericStatsRange& stats,
    const read::DataSkippingFilter& filter
) const {
    if (block_id < stats.first_block_id) {
        return true;
    }

    const auto relative_block_id = block_id - stats.first_block_id;
    if (relative_block_id >= stats.block_count) {
        return true;
    }

    for (const auto& predicate : filter.predicates) {
        const auto column_it = stats.by_column.find(predicate.column_idx);
        if (column_it == stats.by_column.end()) {
            continue;
        }

        if (relative_block_id >= column_it->second.size()) {
            return true;
        }

        if (!block_may_satisfy_predicate(column_it->second[relative_block_id], predicate)) {
            return false;
        }
    }

    return true;
}

}
