#ifndef ATTRACTOR_HANDLERS_FAN_IN_HANDLER_HPP
#define ATTRACTOR_HANDLERS_FAN_IN_HANDLER_HPP

#include <attractor/handler.hpp>

namespace attractor {

class FanInHandler final : public Handler {
  public:
    explicit FanInHandler(CodergenBackend* backend);

    [[nodiscard]] auto execute(const Node& node, Context& ctx, const Graph& graph,
                               const LogsRoot& logs_root) const -> Outcome override;

  private:
    CodergenBackend* m_backend{nullptr};
};

}  // namespace attractor

#endif  // ATTRACTOR_HANDLERS_FAN_IN_HANDLER_HPP
