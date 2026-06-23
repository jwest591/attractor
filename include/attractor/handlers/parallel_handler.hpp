#ifndef ATTRACTOR_HANDLERS_PARALLEL_HANDLER_HPP
#define ATTRACTOR_HANDLERS_PARALLEL_HANDLER_HPP

#include <attractor/engine.hpp>
#include <attractor/handler.hpp>

#include <functional>

namespace attractor {

class ParallelHandler final : public Handler {
public:
    // RunFn: executes a sub-pipeline from start_id; injected for testability.
    // In production, Engine passes [this](g, id, cfg) { return run_from(g, id, cfg); }.
    using RunFn = std::function<Outcome(const Graph&, const NodeId&, const RunConfig&)>;

    explicit ParallelHandler(RunFn run_fn);

    [[nodiscard]] auto execute(const Node& node, Context& ctx, const Graph& graph,
                               const RunConfig& run_config) const -> Outcome override;

private:
    RunFn m_run_fn;
};

}  // namespace attractor

#endif  // ATTRACTOR_HANDLERS_PARALLEL_HANDLER_HPP
