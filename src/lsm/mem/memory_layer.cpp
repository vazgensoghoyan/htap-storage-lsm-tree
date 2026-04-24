#include "lsmtree/mem/memory_layer.hpp"
#include "lsmtree/mem/memory_cursor.hpp"

using namespace htap::lsmtree;
using namespace htap::storage;

MemoryLayer::MemoryLayer()
    : threshold_(DEFAULT_MEMTABLE_THRESHOLD),
      active_(std::make_shared<MemTable>()),
      immutables_()
{}

void MemoryLayer::insert(Key key, const Row& row) {
    active_->insert(key, row);

    if (active_->size() < threshold_) return;
    force_freeze();
}

void MemoryLayer::force_freeze() {
    auto imm = active_->freeze(); 
    immutables_.push_back(std::move(imm));

    active_ = std::make_shared<MemTable>();
}

std::unique_ptr<ICursor> MemoryLayer::get(
    Key key,
    const std::vector<size_t>& projection
) const {
    std::vector<std::shared_ptr<const ImmutableMemTable>> imm_views;

    imm_views.reserve(immutables_.size());
    for (auto& imm : immutables_)
        imm_views.push_back(imm);

    auto cursor = std::make_unique<MemoryCursor>(
        active_,
        imm_views,
        key,
        std::nullopt,
        projection
    );

    return cursor;
}

std::unique_ptr<ICursor> MemoryLayer::scan(
    OptKey from,
    OptKey to,
    const std::vector<size_t>& projection
) const {
    std::vector<std::shared_ptr<const ImmutableMemTable>> imm_views;

    imm_views.reserve(immutables_.size());
    for (auto& imm : immutables_)
        imm_views.push_back(imm);

    return std::make_unique<MemoryCursor>(
        active_,
        imm_views,
        from,
        to,
        projection
    );
}

size_t MemoryLayer::immutable_count() const noexcept {
    return immutables_.size();
}
