#include "lsmtree/mem/memtable_realizations/memtable_factory.hpp"

#include <stdexcept>

using namespace htap::lsmtree;

std::unique_ptr<IMemTable> MemTableFactory::create(MemTableType type) {
    switch (type) {
        case MemTableType::MAP:
            return std::make_unique<MapMemTable>();

        case MemTableType::SKIPLIST:
            return std::make_unique<MapMemTable>();
            //return std::make_unique<SkipListMemTable>();
    }

    throw std::invalid_argument("Unknown MemTableType");
}