#include "lsmtree/mem/memory_layer.hpp"
#include "lsmtree/mem/memory_cursor.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

MemoryLayer::MemoryLayer() : active_(std::make_shared<MemTable>()) {}

void MemoryLayer::insert(Key key, const Row& row) {
    active_->insert(key, row);
    if (active_->size() >= threshold_)
        force_freeze();
}

std::optional<Row> MemoryLayer::get(Key key) const {
    // 1. active (самая новая)
    if (active_) {
        auto v = active_->get(key);
        if (v) return v;
    }

    // 2. immutables (от новых к старым)
    for (auto it = immutables_.rbegin(); it != immutables_.rend(); ++it) {
        auto v = (*it)->get(key);
        if (v) return v;
    }

    return std::nullopt;
}

std::unique_ptr<ICursor> MemoryLayer::scan(
    std::optional<Key> from,
    std::optional<Key> to,
    std::vector<size_t> projection
) const {
    // snapshot через shared_ptr
    std::shared_ptr<const MemTable> active_snapshot = active_;
    std::vector<std::shared_ptr<const ImmutableMemTable>> imm_snapshot;
    imm_snapshot.reserve(immutables_.size());

    for (const auto& imm : immutables_) {
        imm_snapshot.push_back(imm);
    }

    return std::make_unique<MemoryCursor>(
        std::move(active_snapshot),
        std::move(imm_snapshot),
        from,
        to,
        std::move(projection)
    );
}

void MemoryLayer::force_freeze() {
    if (!active_ || active_->size() == 0) return;

    auto&& frozen = active_->extract();
    immutables_.push_back(
        std::make_shared<ImmutableMemTable>(std::move(frozen))
    );

    active_ = std::make_shared<MemTable>();
}

size_t MemoryLayer::immutable_count() const noexcept {
    return immutables_.size();
}
