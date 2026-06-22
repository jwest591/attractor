#ifndef ATTRACTOR_CLI_CLAUDE_HEADLESS_BACKEND_HPP
#define ATTRACTOR_CLI_CLAUDE_HEADLESS_BACKEND_HPP

#include <attractor/handler.hpp>
#include <expected>
#include <filesystem>
#include <string>

namespace attractor {

class ClaudeCodeHeadlessBackend final : public CodergenBackend {
  public:
    ClaudeCodeHeadlessBackend();
    explicit ClaudeCodeHeadlessBackend(std::string claude_exe);
    explicit ClaudeCodeHeadlessBackend(std::filesystem::path logs_root,
                                       std::string claude_exe = "claude");

    [[nodiscard]] auto run(const Node& node, const PromptText& prompt,
                           Context& ctx) const -> std::expected<LlmResponse, Outcome> override;

  private:
    std::string m_claude_exe{"claude"};
    std::filesystem::path m_logs_root{};
    std::string m_scripts_dir;
};

}  // namespace attractor

#endif  // ATTRACTOR_CLI_CLAUDE_HEADLESS_BACKEND_HPP
