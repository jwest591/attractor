#ifndef ATTRACTOR_HANDLER_HPP
#define ATTRACTOR_HANDLER_HPP

#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <expected>
#include <nlohmann/json.hpp>
#include <vector>

namespace attractor {

class Context;  // full definition in context.hpp

// -- Outcome ------------------------------------------------------------------

struct Outcome {
    StageStatus status{StageStatus::success};
    EdgeLabel preferred_label{};
    // NOLINT(readability-redundant-member-init): defaults required to suppress GCC -Wmissing-field-initializers
    std::vector<NodeId> suggested_next_ids{};  // NOLINT(readability-redundant-member-init)
    nlohmann::json context_updates = nlohmann::json::object();
    HandlerNote notes{};                 // NOLINT(readability-redundant-member-init)
    DiagnosticMessage failure_reason{};  // NOLINT(readability-redundant-member-init)

    [[nodiscard]] static auto fail(DiagnosticMessage reason) -> Outcome
    {
        return Outcome{.status = StageStatus::fail, .failure_reason = std::move(reason)};
    }
};

// -- CodergenBackend interface ------------------------------------------------

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class CodergenBackend {
  public:
    virtual ~CodergenBackend() = default;
    [[nodiscard]] virtual auto run(const Node& node, const PromptText& prompt,
                                   Context& ctx) const -> std::expected<LlmResponse, Outcome> = 0;
};

// -- Handler interface --------------------------------------------------------

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class Handler {
  public:
    virtual ~Handler() = default;
    [[nodiscard]] virtual auto execute(const Node& node, Context& ctx, const Graph& graph,
                                       const LogsRoot& logs_root) const -> Outcome = 0;
};

}  // namespace attractor

#endif  // ATTRACTOR_HANDLER_HPP
