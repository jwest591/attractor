#include "claude_headless_backend.hpp"

#include <attractor/handler.hpp>
#include <attractor/types.hpp>

namespace attractor {

auto ClaudeCodeHeadlessBackend::run(const Node& /*node*/, const PromptText& /*prompt*/,
                                     Context& /*ctx*/) const
    -> std::expected<LlmResponse, Outcome>
{
    return std::unexpected(
        Outcome::fail(DiagnosticMessage{"ClaudeCodeHeadlessBackend not yet implemented (Story 5.2)"}));
}

}  // namespace attractor
