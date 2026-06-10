#include <attractor/backends/noop_backend.hpp>

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <expected>
#include <string>
#include <type_safe/strong_typedef.hpp>

namespace attractor {

auto NoOpBackend::run(const Node& node, const PromptText& /*prompt*/,
                      Context& /*ctx*/) const -> std::expected<LlmResponse, Outcome>
{
    return LlmResponse{"[NoOp] Simulated response for node: " + type_safe::get(node.id)};
}

}  // namespace attractor
