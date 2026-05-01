#include "lsmtree/mem/memory_layer.hpp"
#include "utils/logger.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

MemoryLayer::MemoryLayer(size_t threshold) : threshold_(threshold), active_(std::make_unique<MemTable>()) {}

void MemoryLayer::insert(Key key, const Row& row) {
    active_->insert(key, row);

    if (active_->size() < threshold_) return;
    force_freeze();
}

void MemoryLayer::force_freeze() {
    auto imm = active_->freeze(); 
    immutables_.push_back(std::move(imm));

    size_t size = immutables_.back()->size();
    LOG_INFO("MemTable frozen -> ImmutableMemTable created with {} rows", size);

    active_ = std::make_unique<MemTable>();
}

size_t MemoryLayer::immutable_count() const {
    return immutables_.size();
}
