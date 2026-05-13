#pragma once // lsmtree/mem/memtable_realizations/memtable_factory.hpp

#include <memory>

#include "lsmtree/mem/memtable_interface.hpp"

#include "lsmtree/mem/memtable_realizations/map_memtable.hpp"
//#include "lsmtree/mem/memtable_realization/skiplist_memtable.hpp"

namespace htap::lsmtree {

enum class MemTableType {
    MAP,
    SKIPLIST
};

class MemTableFactory {
public:
    static std::unique_ptr<IMemTable> create(MemTableType type);
};

} // namespace htap::lsmtree
