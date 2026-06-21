#ifndef ATTRACTOR_CLI_CLAUDE_TMUX_BACKEND_HPP
#define ATTRACTOR_CLI_CLAUDE_TMUX_BACKEND_HPP

#include <attractor/handler.hpp>
#include <expected>
#include <mutex>
#include <string>
#include <unordered_map>

namespace attractor {

class ClaudeCodeTmuxBackend final : public CodergenBackend {
  public:
    ClaudeCodeTmuxBackend() = default;
    explicit ClaudeCodeTmuxBackend(std::string tmux_bin, std::string run_id);
    ~ClaudeCodeTmuxBackend();

    [[nodiscard]] auto run(const Node& node, const PromptText& prompt, Context& ctx) const
        -> std::expected<LlmResponse, Outcome> override;

  private:
    std::string _tmux_bin{"tmux"};
    std::string _session_id{"attractor_claude"};
};

}  // namespace attractor

#endif  // ATTRACTOR_CLI_CLAUDE_TMUX_BACKEND_HPP
