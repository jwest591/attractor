#ifndef ATTRACTOR_HANDLERS_WAIT_FOR_HUMAN_HANDLER_HPP
#define ATTRACTOR_HANDLERS_WAIT_FOR_HUMAN_HANDLER_HPP

#include <attractor/handler.hpp>
#include <attractor/interviewer.hpp>

namespace attractor {

class WaitForHumanHandler final : public Handler {
  public:
    explicit WaitForHumanHandler(Interviewer& interviewer);

    [[nodiscard]] auto execute(const Node& node, Context& ctx, const Graph& graph,
                               const RunConfig& run_config) const -> Outcome override;

  private:
    Interviewer* m_interviewer;  // non-owning; caller manages lifetime
};

}  // namespace attractor

#endif  // ATTRACTOR_HANDLERS_WAIT_FOR_HUMAN_HANDLER_HPP
