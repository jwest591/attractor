#ifndef ATTRACTOR_HANDLERS_TOOL_HANDLER_HPP
#define ATTRACTOR_HANDLERS_TOOL_HANDLER_HPP

#include <attractor/handler.hpp>
#include <functional>
#include <string>
#include <string_view>

namespace attractor {

class ToolHandler final : public Handler {
  public:
    // CommandRunner: callable that executes a shell command and returns its stdout.
    // Inject in tests to avoid spawning real processes (ASR-2).
    // When omitted, defaults to popen-based shell execution.
    using CommandRunner = std::function<std::string(std::string_view)>;

    explicit ToolHandler(CommandRunner runner = {});

    [[nodiscard]] auto execute(const Node& node, Context& ctx, const Graph& graph,
                               const LogsRoot& logs_root) const -> Outcome override;

  private:
    CommandRunner m_runner;
};

}  // namespace attractor

#endif  // ATTRACTOR_HANDLERS_TOOL_HANDLER_HPP
