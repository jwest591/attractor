#include "claude_tmux_backend.hpp"

#include <attractor/handler.hpp>
#include <attractor/types.hpp>

namespace attractor {

auto ClaudeCodeTmuxBackend::run(const Node& /*node*/, const PromptText& /*prompt*/,
                                 Context& /*ctx*/) const
    -> std::expected<LlmResponse, Outcome>
{
    return std::unexpected(
        Outcome::fail(DiagnosticMessage{"ClaudeCodeTmuxBackend not yet implemented (Story 5.3)"}));
}

}  // namespace attractor
