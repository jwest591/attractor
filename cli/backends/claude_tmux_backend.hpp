#ifndef ATTRACTOR_CLI_CLAUDE_TMUX_BACKEND_HPP
#define ATTRACTOR_CLI_CLAUDE_TMUX_BACKEND_HPP

#include <attractor/handler.hpp>
#include <expected>
#include <filesystem>
#include <string>

namespace attractor {

class ClaudeCodeTmuxBackend final : public CodergenBackend {
  public:
    explicit ClaudeCodeTmuxBackend(std::string tmux_bin, std::filesystem::path logs_root);
    ~ClaudeCodeTmuxBackend();

    ClaudeCodeTmuxBackend(const ClaudeCodeTmuxBackend&) = delete;
    ClaudeCodeTmuxBackend& operator=(const ClaudeCodeTmuxBackend&) = delete;
    ClaudeCodeTmuxBackend(ClaudeCodeTmuxBackend&&) = delete;
    ClaudeCodeTmuxBackend& operator=(ClaudeCodeTmuxBackend&&) = delete;

    [[nodiscard]] auto run(const Node& node, const PromptText& prompt, Context& ctx) const
        -> std::expected<LlmResponse, Outcome> override;

  private:
    std::string m_tmux_bin{"tmux"};
    std::string m_session_id{};
    std::filesystem::path m_logs_root{};
    int m_context_critical_pct{85};
};

}  // namespace attractor

#endif  // ATTRACTOR_CLI_CLAUDE_TMUX_BACKEND_HPP
