#ifndef ATTRACTOR_HANDLERS_CODERGEN_HANDLER_HPP
#define ATTRACTOR_HANDLERS_CODERGEN_HANDLER_HPP

#include <attractor/handler.hpp>

namespace attractor {

class CodergenHandler final : public Handler {
  public:
    explicit CodergenHandler(CodergenBackend* backend = nullptr);

    [[nodiscard]] auto execute(const Node& node, Context& ctx, const Graph& graph,
                               const RunConfig& run_config) const -> Outcome override;

  private:
    CodergenBackend* m_backend{nullptr};
};

}  // namespace attractor

#endif  // ATTRACTOR_HANDLERS_CODERGEN_HANDLER_HPP
