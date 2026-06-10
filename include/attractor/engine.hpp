#ifndef ATTRACTOR_ENGINE_HPP
#define ATTRACTOR_ENGINE_HPP

#include <attractor/graph.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/types.hpp>

namespace attractor {

struct RunConfig {
    LogsRoot logs_root;
};

class Engine {
  public:
    // Default constructor: registers StartHandler, ExitHandler, ConditionalHandler,
    // and CodergenHandler(NoOpBackend) for the 4 built-in simulation types.
    Engine();

    explicit Engine(HandlerRegistry registry);

    // Runs the pipeline. Catches all handler exceptions at the boundary.
    // Returns Outcome{success} if the pipeline terminates normally at the exit node
    // with all goal gates satisfied. Returns Outcome::fail(...) otherwise.
    [[nodiscard]] auto run(const Graph& graph, const RunConfig& config) const -> Outcome;

  private:
    HandlerRegistry m_registry;

    [[nodiscard]] auto run_from(const Graph& graph, const NodeId& start_id, const RunConfig& config) const -> Outcome;
};

}  // namespace attractor

#endif  // ATTRACTOR_ENGINE_HPP
