#pragma once

#include "lsmtree/sstable/metadata/sstable_info.hpp"
#include "storage/api/cursor_interface.hpp"
#include "storage/api/types.hpp"
#include "storage/read/data_skipping_filter.hpp"
#include "storage/read/sstable/key_range.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace htap::storage::read::sstable {

std::unique_ptr<ICursor> make_sstable_cursor(
    const lsmtree::sstable::SSTableInfo& info,
    const KeyRange& range,
    const std::vector<ValueType>& schema,
    const std::vector<std::size_t>& projection,
    const read::DataSkippingFilter& data_skipping_filter = {}
);

} 
