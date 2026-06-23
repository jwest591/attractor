#ifndef ATTRACTOR_HANDLERS_MANAGER_LOOP_HANDLER_HPP
#define ATTRACTOR_HANDLERS_MANAGER_LOOP_HANDLER_HPP

#include <attractor/handler.hpp>

namespace attractor {

class ManagerLoopHandler final : public Handler {
  public:
    [[nodiscard]] auto execute(const Node& node, Context& ctx, const Graph& graph,
                               const RunConfig& run_config) const -> Outcome override;
};

}  // namespace attractor

#endif  // ATTRACTOR_HANDLERS_MANAGER_LOOP_HANDLER_HPP
