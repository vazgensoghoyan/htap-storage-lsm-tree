#include "lsmtree/mem/memory_layer.hpp"
#include "utils/logger.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

MemoryLayer::MemoryLayer(MemTableType memtable_type, size_t threshold)
    : memtable_type_(memtable_type)
    , threshold_(threshold)
    , active_(MemTableFactory::create(memtable_type_)) 
{}

size_t MemoryLayer::immutable_count() const {
    return immutables_.size();
}

void MemoryLayer::insert(const Row& row) {
    active_->insert(row);

    if (active_->size() < threshold_) return;
    force_freeze();
}

void MemoryLayer::force_freeze() {
    auto imm = active_->to_sorted_immutable();
    immutables_.push_back(std::move(imm));

    size_t size = immutables_.back()->size();
    LOG_INFO("MemTable frozen -> ImmutableMemTable created with {} rows", size);

    active_ = MemTableFactory::create(memtable_type_);
}
