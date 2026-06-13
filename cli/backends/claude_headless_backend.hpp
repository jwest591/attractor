#ifndef ATTRACTOR_CLI_CLAUDE_HEADLESS_BACKEND_HPP
#define ATTRACTOR_CLI_CLAUDE_HEADLESS_BACKEND_HPP

#include <attractor/handler.hpp>
#include <expected>

namespace attractor {

class ClaudeCodeHeadlessBackend final : public CodergenBackend {
  public:
    [[nodiscard]] auto run(const Node& node, const PromptText& prompt,
                           Context& ctx) const -> std::expected<LlmResponse, Outcome> override;
};

}  // namespace attractor

#endif  // ATTRACTOR_CLI_CLAUDE_HEADLESS_BACKEND_HPP
