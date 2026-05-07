#include "storage/cursor/cursor_factory.hpp"
#include "storage/cursor/chain_cursor.hpp"
#include "storage/cursor/empty_cursor.hpp"
#include "storage/cursor/merge_cursor.hpp"

#include <memory>
#include <vector>
#include <stdexcept>

namespace htap::storage::cursor {

std::unique_ptr<ICursor> compose_cursors(
    std::vector<std::unique_ptr<ICursor>> cursors,
    ScanOrder order
) {

    std::vector<std::unique_ptr<ICursor>> valid_cursors;
    valid_cursors.reserve(cursors.size());

    for (auto& cursor : cursors) {
        if (cursor != nullptr && cursor->valid()) {
            valid_cursors.push_back(std::move(cursor));
        }
    }

    if (valid_cursors.empty()) {
        return std::make_unique<EmptyCursor>();
    }

    if (valid_cursors.size() == 1) {
        return std::move(valid_cursors.front());
    }

    switch (order) {
        case ScanOrder::Unordered:
            return std::make_unique<ChainCursor>(std::move(valid_cursors)); 

        case ScanOrder::KeyAscending:
            return std::make_unique<MergeCursor>(std::move(valid_cursors)); 
    }

    throw std::logic_error("Unsupported ScanOrder type");
}

} 