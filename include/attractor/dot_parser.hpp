#ifndef ATTRACTOR_DOT_PARSER_HPP
#define ATTRACTOR_DOT_PARSER_HPP

#include <attractor/graph.hpp>
#include <expected>
#include <string_view>

namespace attractor {

struct ParseError {
    std::string message;
    int line{0};
    int column{0};
};

[[nodiscard]] auto parse_graph(std::string_view source) -> std::expected<Graph, ParseError>;

}  // namespace attractor

#endif  // ATTRACTOR_DOT_PARSER_HPP
