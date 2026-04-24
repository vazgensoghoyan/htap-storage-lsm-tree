#include "lsmtree/mem/memory_cursor.hpp"

#include <stdexcept>
#include <limits>

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
    advance(); // выставляем первый элемент
}

bool MemoryCursor::valid() const {
    return valid_;
}

void MemoryCursor::next() {
    if (!valid_) return;
    advance();
}

Key MemoryCursor::key() const {
    if (!valid_)
        throw std::runtime_error("Cursor not valid");

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
    if (projection_.empty()) return true;

    for (auto c : projection_)
        if (c == column_idx)
            return true;

    return false;
}

/**
 * Инициализируем итераторы по слоям
 * порядок:
 * 0 = active (newest)
 * 1..N = immutables (older → older)
 */
void MemoryCursor::init_sources(
    const std::shared_ptr<const MemTable>& active,
    const std::vector<std::shared_ptr<const ImmutableMemTable>>& immutables,
    std::optional<Key> from
) {
    sources_.clear();

    size_t priority = 0;

    // ACTIVE (самый новый)
    if (active) {
        auto it = from ? active->lower_bound(*from) : active->begin();

        sources_.push_back(Source{
            .it = it,
            .end = active->end(),
            .priority = priority++
        });
    }

    // IMMUTABLES (новые → старые)
    for (auto it = immutables.rbegin(); it != immutables.rend(); ++it) {
        auto& imm = *it;

        auto iter = from ? imm->lower_bound(*from) : imm->begin();

        sources_.push_back(Source{
            .it = iter,
            .end = imm->end(),
            .priority = priority++
        });
    }
}

/**
 * Основная логика:
 * - выбираем минимальный key среди всех источников
 * - активный слой имеет приоритет (latest-wins)
 * - дедуп делаем через пропуск одинаковых key
 */
void MemoryCursor::advance() {
    valid_ = false;
    current_row_ = nullptr;

    while (true) {
        Key best_key = std::numeric_limits<Key>::max();
        size_t best_idx = sources_.size();
        bool found = false;

        // 1. найти минимальный key среди всех источников
        for (size_t i = 0; i < sources_.size(); ++i) {
            auto& src = sources_[i];

            if (src.it == src.end)
                continue;

            Key k = src.it->first;

            if (to_ && k >= *to_)
                continue;

            if (!found || k < best_key) {
                best_key = k;
                best_idx = i;
                found = true;
            }
        }

        if (!found)
            return; // конец

        auto& src = sources_[best_idx];

        current_key_ = best_key;
        current_row_ = &src.it->second;
        valid_ = true;

        // двигаем выбранный источник
        ++src.it;

        // skip duplicates (все остальные источники с тем же key)
        for (auto& s : sources_) {
            while (s.it != s.end && s.it->first == best_key) {
                ++s.it;
            }
        }

        return;
    }
}
