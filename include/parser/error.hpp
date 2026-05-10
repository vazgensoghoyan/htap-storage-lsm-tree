#pragma once

#include "parser/token.hpp"
#include <stdexcept>

namespace htap::parser {
    
class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, SourcePosition position)
        : std::runtime_error(message),
          position_(position) {
    }

    const SourcePosition& position() const noexcept {
        return position_;
    }

private:
    SourcePosition position_;
};

}