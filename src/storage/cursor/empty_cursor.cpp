#include "storage/cursor/empty_cursor.hpp"

#include <stdexcept>

namespace htap::storage::cursor {

bool EmptyCursor::valid() const {
    return false;
}

void EmptyCursor::next() {
}

Key EmptyCursor::key() const {
    throw std::logic_error("key() called on invalid cursor");
}

NullableValue EmptyCursor::value(std::size_t column_idx) const {
    (void)column_idx;
    throw std::logic_error("value() called on invalid cursor");
}

}