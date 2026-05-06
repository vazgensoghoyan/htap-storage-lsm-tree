#include "lsmtree/mem/imm_memtable.hpp"
#include "utils/logger.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

ImmutableMemTable::ImmutableMemTable(std::vector<Row>&& data) : data_(std::move(data)) {}

size_t ImmutableMemTable::size() const {
    return data_.size();
}
