#pragma once

#include "storage/api/types.hpp"

namespace htap::storage::read::sstable {

struct KeyRange {
    OptKey from;
    OptKey to;
};

inline bool contains(const KeyRange& range, Key key) {
    if (range.from.has_value() && key < *range.from) {
        return false;
    }

    if (range.to.has_value() && key >= *range.to) {
        return false;
    }

    return true;
}

}