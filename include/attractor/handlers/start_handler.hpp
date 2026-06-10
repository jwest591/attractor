#ifndef ATTRACTOR_HANDLERS_START_HANDLER_HPP
#define ATTRACTOR_HANDLERS_START_HANDLER_HPP

#include <attractor/handler.hpp>

namespace attractor {

class StartHandler final : public Handler {
  public:
    [[nodiscard]] auto execute(const Node& node, Context& ctx, const Graph& graph,
                               const LogsRoot& logs_root) const -> Outcome override;
};

}  // namespace attractor

#endif  // ATTRACTOR_HANDLERS_START_HANDLER_HPP
