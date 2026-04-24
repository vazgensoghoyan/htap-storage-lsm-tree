#include "lsmtree/mem/memory_layer.hpp"
#include "lsmtree/mem/cursors/merge_cursor.hpp"
#include "utils/logger.hpp"

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

    size_t size = immutables_.back()->size();
    LOG_INFO("MemTable frozen -> ImmutableMemTable created with {} rows", size);

    active_ = std::make_shared<MemTable>();
}

std::unique_ptr<ICursor> MemoryLayer::get(
    Key key,
    const std::vector<size_t>& projection
) const {
    std::vector<std::unique_ptr<ICursor>> cursors;

    if (auto c = active_->get(key, projection))
        cursors.push_back(std::move(c));

    for (auto it = immutables_.rbegin(); it != immutables_.rend(); ++it) // от новых к старым
        if (auto c = (*it)->get(key, projection))
            cursors.push_back(std::move(c));

    if (cursors.empty())
        return nullptr;

    return std::make_unique<MergeCursor>(std::move(cursors));
}

std::unique_ptr<ICursor> MemoryLayer::scan(
    OptKey from,
    OptKey to,
    const std::vector<size_t>& projection
) const {
    std::vector<std::unique_ptr<ICursor>> cursors;

    cursors.push_back(active_->scan(from, to, projection));
    for (auto it = immutables_.rbegin(); it != immutables_.rend(); ++it)
        cursors.push_back((*it)->scan(from, to, projection));

    return std::make_unique<MergeCursor>(std::move(cursors));
}

size_t MemoryLayer::immutable_count() const noexcept {
    return immutables_.size();
}
