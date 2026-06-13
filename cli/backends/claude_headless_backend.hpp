#ifndef ATTRACTOR_CLI_CLAUDE_HEADLESS_BACKEND_HPP
#define ATTRACTOR_CLI_CLAUDE_HEADLESS_BACKEND_HPP

#include <attractor/handler.hpp>
#include <expected>
#include <string>

namespace attractor {

class ClaudeCodeHeadlessBackend final : public CodergenBackend {
  public:
    ClaudeCodeHeadlessBackend() = default;
    explicit ClaudeCodeHeadlessBackend(std::string claude_exe);

    [[nodiscard]] auto run(const Node& node, const PromptText& prompt,
                           Context& ctx) const -> std::expected<LlmResponse, Outcome> override;

  private:
    std::string m_claude_exe{"claude"};
};

}  // namespace attractor

#endif  // ATTRACTOR_CLI_CLAUDE_HEADLESS_BACKEND_HPP
