#pragma once

#include "storage/api/cursor_interface.hpp"
#include "storage/api/storage_engine_interface.hpp"

#include <memory>
#include <vector>

namespace htap::storage::cursor {

std::unique_ptr<ICursor> compose_cursors(
    std::vector<std::unique_ptr<ICursor>> cursors,
    ScanOrder order
);

} 