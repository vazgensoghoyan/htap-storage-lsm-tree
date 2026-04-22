#include "lsmtree/mem/memory_layer.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

MemoryLayer::MemoryLayer(size_t threshold)
    : threshold_(threshold),
      active_(std::make_unique<MemTable>()) {}

void MemoryLayer::insert(Key key, const Row& row) {
    active_->insert(key, row);
    maybe_freeze();
}

std::optional<Row> MemoryLayer::get(Key key) const {
    // 1. active
    if (active_) {
        auto v = active_->get(key);
        if (v) return v;
    }

    // 2. immutables (сначала новые)
    for (auto it = immutables_.rbegin(); it != immutables_.rend(); ++it) {
        auto v = (*it)->get(key);
        if (v) return v;
    }

    return std::nullopt;
}

void MemoryLayer::maybe_freeze() {
    if (!active_ || active_->size() < threshold_) return;

    force_freeze();
}

void MemoryLayer::force_freeze() {
    if (!active_ || active_->size() == 0) return;

    auto frozen = active_->freeze();
    immutables_.push_back(
        std::make_unique<ImmutableMemTable>(std::move(frozen))
    );

    active_ = std::make_unique<MemTable>();
}
