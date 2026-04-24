#include "lsmtree/mem/memory_cursor.hpp"

#include <stdexcept>

using namespace htap::lsmtree;
using namespace htap::storage;

MemoryCursor::MemoryCursor(
    std::shared_ptr<const MemTable> active,
    std::vector<std::shared_ptr<const ImmutableMemTable>> immutables,
    OptKey from,
    OptKey to,
    std::vector<size_t> projection
)
    : projection_(std::move(projection)),
      to_(to),
      sources_()
{
    init_sources(active, immutables, from);
    advance();
}

void MemoryCursor::init_sources(
    const std::shared_ptr<const MemTable>& active,
    const std::vector<std::shared_ptr<const ImmutableMemTable>>& immutables,
    OptKey from
) {
    sources_.clear();

    size_t idx = 0;

    // ACTIVE (priority 0)
    {
        auto cur = active->scan(from, std::nullopt, projection_);
        if (cur->valid()) {
            sources_.push_back({std::move(cur), 0, true});
            heap_.push({sources_.back().cursor->key(), idx});
        }
        ++idx;
    }

    // IMMUTABLES (priority increasing)
    size_t priority = 1;
    for (auto& imm : immutables) {
        auto cur = imm->scan(from, std::nullopt, projection_);
        if (cur->valid()) {
            sources_.push_back({std::move(cur), priority, true});
            heap_.push({sources_.back().cursor->key(), idx});
        }
        ++idx;
        ++priority;
    }
}

bool MemoryCursor::valid() const {
    return valid_;
}

Key MemoryCursor::key() const {
    if (!valid_)
        throw std::runtime_error("MemoryCursor invalid");
    return current_key_;
}

NullableValue MemoryCursor::value(size_t column_idx) const {
    if (!valid_)
        throw std::runtime_error("MemoryCursor invalid");

    return current_row_->at(column_idx);
}

void MemoryCursor::next() {
    if (!valid_) return;
    advance();
}

void MemoryCursor::advance() {
    valid_ = false;
    current_row_ = nullptr;

    while (!heap_.empty()) {
        auto top = heap_.top();
        heap_.pop();

        auto& src = sources_[top.source_idx];
        if (!src.cursor || !src.cursor->valid())
            continue;

        Key k = src.cursor->key();

        src.cursor->next();

        if (src.cursor->valid()) {
            heap_.push({src.cursor->key(), top.source_idx});
        }

        // дедуп + latest wins:
        current_key_ = k;
        current_row_ = &src.cursor->row();

        valid_ = true;
        return;
    }
}
