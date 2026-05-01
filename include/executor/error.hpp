#pragma once

#include <stdexcept>
#include <string>

namespace htap::executor {

class BindError : public std::runtime_error {
public:
    explicit BindError(const std::string& message)
        : std::runtime_error(message) {
    }
};

class TableNotFoundError : public BindError {
public:
    explicit TableNotFoundError(const std::string& table_name)
        : BindError("Table not found: " + table_name) {
    }
};

class ColumnNotFoundError : public BindError {
public:
    explicit ColumnNotFoundError(const std::string& column_name)
        : BindError("Column not found: " + column_name) {
    }
};

class TypeMismatchError : public BindError {
public:
    explicit TypeMismatchError(const std::string& message)
        : BindError(message) {
    }
};

class InvalidQueryError : public BindError {
public:
    explicit InvalidQueryError(const std::string& message)
        : BindError(message) {
    }
};

}