#pragma once // storage/interfaces/readable_table_interface.hpp

#include <memory>
#include <optional>
#include <vector>

#include "storage/api/cursor_interface.hpp"
#include "storage/api/types.hpp"

namespace htap::lsmtree {

class IReadableTable {
public:
    virtual ~IReadableTable() = default;

    // передача projection = {} означает что захватываем все колонки

    virtual std::unique_ptr<storage::ICursor> get(
        storage::Key key,
        const std::vector<size_t>& projection) const = 0;

    // [from, to)
    // решил, что так лучше, как например для складывания диапозонов
    // И любую из границ можем не указывать, это эквивалентно +-inf
    virtual std::unique_ptr<storage::ICursor> scan(
        storage::OptKey from,
        storage::OptKey to,
        const std::vector<size_t>& projection) const = 0;
};

} // namespace htap::lsmtree
