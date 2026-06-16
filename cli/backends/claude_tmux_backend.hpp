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
    explicit ClaudeCodeTmuxBackend(std::string tmux_bin);
    ~ClaudeCodeTmuxBackend();

    [[nodiscard]] auto run(const Node& node, const PromptText& prompt,
                           Context& ctx) const -> std::expected<LlmResponse, Outcome> override;

  private:
    std::string m_tmux_bin{"tmux"};
    mutable std::mutex m_mutex;
    mutable std::unordered_map<std::string, std::string> m_sessions;  // name -> transcript_path
};

}  // namespace attractor

#endif  // ATTRACTOR_CLI_CLAUDE_TMUX_BACKEND_HPP
