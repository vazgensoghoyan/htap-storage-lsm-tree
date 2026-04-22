#include "lsmtree/mem/memory_cursor.hpp"

#include <algorithm>
#include <stdexcept>

using namespace htap::lsmtree;
using namespace htap::storage;

MemoryCursor::MemoryCursor(
    std::shared_ptr<const MemTable> active,
    std::vector<std::shared_ptr<const ImmutableMemTable>> immutables,
    std::optional<Key> from,
    std::optional<Key> to,
    std::vector<size_t> projection
)
: projection_(std::move(projection)),
  to_(to),
  active_(std::move(active)),
  immutables_(std::move(immutables)) {

    init_sources(active_, immutables_, from);
    build_heap();
    advance();
}

bool MemoryCursor::valid() const {
    return valid_;
}

void MemoryCursor::next() {
    if (!valid_) return;
    advance();
}

Key MemoryCursor::key() const {
    if (!valid_) {
        throw std::runtime_error("Cursor not valid");
    }
    return current_key_;
}

NullableValue MemoryCursor::value(size_t column_idx) const {
    if (!valid_)
        throw std::runtime_error("Cursor not valid");

    if (!is_projected(column_idx))
        throw std::runtime_error("Column not in projection");

    return (*current_row_)[column_idx];
}

bool MemoryCursor::is_projected(size_t column_idx) const {
    if (projection_.empty()) return true; // пустой = все колонки

    return std::find(
        projection_.begin(),
        projection_.end(),
        column_idx
    ) != projection_.end();
}

void MemoryCursor::init_sources(
    const std::shared_ptr<const MemTable>& active,
    const std::vector<std::shared_ptr<const ImmutableMemTable>>& immutables,
    std::optional<Key> from
) {
    sources_.clear();

    size_t priority = 0;

    // active (самый новый)
    if (active) {
        auto it = from ? active->lower_bound(*from) : active->begin();

        sources_.push_back(Source{
            .it = it,
            .end = active->end(),
            .priority = priority++
        });
    }

    // immutables (новые → старые)
    for (auto it_imm = immutables.rbegin(); it_imm != immutables.rend(); ++it_imm) {
        auto& imm = *it_imm;

        auto it = from ? imm->lower_bound(*from) : imm->begin();

        sources_.push_back(Source{
            .it = it,
            .end = imm->end(),
            .priority = priority++
        });
    }
}

void MemoryCursor::build_heap() {
    while (!heap_.empty()) heap_.pop();

    for (size_t i = 0; i < sources_.size(); ++i) {
        const auto& src = sources_[i];
        if (src.it != src.end) {
            heap_.push(HeapItem{
                .key = src.it->first,
                .source_idx = i
            });
        }
    }
}

void MemoryCursor::advance() {
    current_row_ = nullptr;
    valid_ = false;

    while (!heap_.empty()) {
        auto item = heap_.top();
        heap_.pop();

        auto& src = sources_[item.source_idx];

        Key k = item.key;

        // range check
        if (to_ && k >= *to_) {
            heap_ = {}; // очистить
            return;
        }

        // это самая новая версия (из-за порядка sources)
        current_key_ = k;
        current_row_ = &src.it->second;
        valid_ = true;

        // двигаем этот источник
        ++src.it;
        if (src.it != src.end) {
            heap_.push(HeapItem{
                .key = src.it->first,
                .source_idx = item.source_idx
            });
        }

        // удалить дубликаты
        skip_duplicates(k);

        return;
    }
}

void MemoryCursor::skip_duplicates(Key k) {
    while (!heap_.empty()) {
        auto item = heap_.top();

        if (item.key != k) break;

        heap_.pop();

        auto& src = sources_[item.source_idx];

        ++src.it;
        if (src.it != src.end) {
            heap_.push(HeapItem{
                .key = src.it->first,
                .source_idx = item.source_idx
            });
        }
    }
}
