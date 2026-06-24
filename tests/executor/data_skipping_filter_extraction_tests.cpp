#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "executor/binder.hpp"
#include "executor/executor.hpp"
#include "parser/ast.hpp"
#include "storage/api/storage_engine_interface.hpp"
#include "storage/cursor/empty_cursor.hpp"
#include "storage/model/schema_builder.hpp"

using namespace htap;

namespace {

class CapturingStorageEngine final : public storage::IStorageEngine {
public:
    CapturingStorageEngine()
        : schema_(storage::SchemaBuilder()
            .add_column("id", storage::ValueType::INT64, true, false)
            .add_column("age", storage::ValueType::INT64, false, true)
            .add_column("score", storage::ValueType::DOUBLE, false, true)
            .add_column("name", storage::ValueType::STRING, false, true)
            .build()) {
    }

    void create_table(const std::string&, const storage::Schema&) override {
    }

    bool table_exists(const std::string& table_name) const override {
        return table_name == "users";
    }

    const storage::Schema& get_table_schema(const std::string&) const override {
        return schema_;
    }

    void insert(const std::string&, const storage::Row&) override {
    }

    std::unique_ptr<storage::ICursor> get(
        const std::string&,
        storage::Key,
        const std::vector<std::size_t>&
    ) const override {
        return std::make_unique<storage::cursor::EmptyCursor>();
    }

    std::unique_ptr<storage::ICursor> scan(
        const std::string&,
        std::optional<storage::Key>,
        std::optional<storage::Key>,
        const std::vector<std::size_t>&,
        storage::ScanOrder,
        const storage::read::DataSkippingFilter& data_skipping_filter
    ) const override {
        last_filter = data_skipping_filter;
        return std::make_unique<storage::cursor::EmptyCursor>();
    }

    storage::Schema schema_;
    mutable storage::read::DataSkippingFilter last_filter;
};

std::unique_ptr<parser::Expression> column(const std::string& name) {
    auto expr = std::make_unique<parser::ColumnExpression>();
    expr->column_name = name;
    return expr;
}

std::unique_ptr<parser::Expression> int_literal(std::int64_t value) {
    auto expr = std::make_unique<parser::IntLiteral>();
    expr->value = value;
    return expr;
}

std::unique_ptr<parser::Expression> double_literal(double value) {
    auto expr = std::make_unique<parser::DoubleLiteral>();
    expr->value = value;
    return expr;
}

std::unique_ptr<parser::Expression> string_literal(std::string value) {
    auto expr = std::make_unique<parser::StringLiteral>();
    expr->value = std::move(value);
    return expr;
}

std::unique_ptr<parser::Expression> binary(
    parser::BinaryOperation operation,
    std::unique_ptr<parser::Expression> left,
    std::unique_ptr<parser::Expression> right
) {
    auto expr = std::make_unique<parser::BinaryExpression>();
    expr->operation = operation;
    expr->left = std::move(left);
    expr->right = std::move(right);
    return expr;
}

std::unique_ptr<parser::SelectItem> select_item(std::unique_ptr<parser::Expression> expression) {
    auto item = std::make_unique<parser::SelectItemExpression>();
    item->expression = std::move(expression);
    return item;
}

}

TEST(DataSkippingFilterExtractionTest, ExtractsOnlySupportedNonKeyNumericPredicates) {
    CapturingStorageEngine storage;
    executor::Binder binder(storage);
    executor::Executor executor(storage);

    parser::SelectStatement statement;
    statement.table_name = "users";
    statement.select_items.push_back(select_item(column("id")));
    statement.where_expression = binary(
        parser::BinaryOperation::And,
        binary(
            parser::BinaryOperation::And,
            binary(
                parser::BinaryOperation::GreaterEqual,
                column("age"),
                int_literal(18)
            ),
            binary(
                parser::BinaryOperation::Less,
                double_literal(10.0),
                column("score")
            )
        ),
        binary(
            parser::BinaryOperation::And,
            binary(
                parser::BinaryOperation::Equal,
                column("name"),
                string_literal("Ann")
            ),
            binary(
                parser::BinaryOperation::Greater,
                column("id"),
                int_literal(0)
            )
        )
    );

    const auto bound = binder.Bind(statement);
    (void)executor.Execute(*bound);

    ASSERT_EQ(storage.last_filter.predicates.size(), 2u);

    EXPECT_EQ(storage.last_filter.predicates[0].column_idx, 1u);
    EXPECT_EQ(storage.last_filter.predicates[0].op, storage::read::NumericComparisonOp::GreaterEqual);
    EXPECT_EQ(std::get<std::int64_t>(storage.last_filter.predicates[0].value), 18);

    EXPECT_EQ(storage.last_filter.predicates[1].column_idx, 2u);
    EXPECT_EQ(storage.last_filter.predicates[1].op, storage::read::NumericComparisonOp::Greater);
    EXPECT_DOUBLE_EQ(std::get<double>(storage.last_filter.predicates[1].value), 10.0);
}
