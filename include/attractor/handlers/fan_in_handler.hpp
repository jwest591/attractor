#ifndef ATTRACTOR_HANDLERS_FAN_IN_HANDLER_HPP
#define ATTRACTOR_HANDLERS_FAN_IN_HANDLER_HPP

#include <attractor/handler.hpp>

#include <memory>

namespace attractor {

class FanInHandler final : public Handler {
  public:
    explicit FanInHandler(std::shared_ptr<CodergenBackend> backend);

    [[nodiscard]] auto execute(const Node& node, Context& ctx, const Graph& graph,
                               const LogsRoot& logs_root) const -> Outcome override;

  private:
    std::shared_ptr<CodergenBackend> m_backend;
};

}  // namespace attractor

#endif  // ATTRACTOR_HANDLERS_FAN_IN_HANDLER_HPP
