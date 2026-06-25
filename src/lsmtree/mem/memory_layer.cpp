#include "lsmtree/mem/memory_layer.hpp"
#include "utils/logger.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

MemoryLayer::MemoryLayer(size_t threshold) : threshold_(threshold), active_(std::make_unique<MemTable>()) {}

void MemoryLayer::insert(const Row& row) {
    active_->insert(row);

    if (active_->size() < threshold_) return;
    active_to_immutable();
}

void MemoryLayer::active_to_immutable() {
    auto imm = active_->to_sorted_immutable();
#if HTAP_ENABLE_LOGGING
    const size_t size = imm->size();
#endif

    immutables_.push_back(ImmPtr(std::move(imm)));

    LOG_INFO("MemTable frozen -> ImmutableMemTable created with {} rows", size);

    active_ = std::make_unique<MemTable>();
}

size_t MemoryLayer::immutable_count() const {
    return immutables_.size();
}

MemoryLayer::ImmPtr MemoryLayer::front_immutable() const {
    if (immutables_.empty()) return nullptr;
    return immutables_.front();
}

MemoryLayer::ImmPtr MemoryLayer::pop_front_immutable() {
    if (immutables_.empty()) return nullptr;

    auto imm = immutables_.front();
    immutables_.pop_front();

    LOG_INFO("ImmutableMemTable popped");

    return imm;
}

const MemTable& MemoryLayer::active() const noexcept {
    return *active_;
}

const std::deque<MemoryLayer::ImmPtr>& MemoryLayer::immutables() const noexcept {
    return immutables_;
}
