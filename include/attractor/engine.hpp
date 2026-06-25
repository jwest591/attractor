#ifndef ATTRACTOR_ENGINE_HPP
#define ATTRACTOR_ENGINE_HPP

#include <attractor/events.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/run_config.hpp>
#include <attractor/types.hpp>

#include <memory>

namespace attractor {

class Engine {
  public:
    // Default constructor: registers StartHandler, ExitHandler, ConditionalHandler,
    // CodergenHandler(NoOpBackend), WaitForHumanHandler, ParallelHandler, and FanInHandler
    // for the built-in types.
    Engine();

    explicit Engine(HandlerRegistry registry);
    Engine(HandlerRegistry registry, EventObserver on_event);
    // Uses default handlers (start, exit, codergen/NoOp, conditional) + provided event observer.
    // Equivalent to Engine() + setting the observer; avoids duplicating default handler setup in callers.
    explicit Engine(EventObserver on_event);

    // Uses all default handlers with the provided backend instead of NoOpBackend.
    explicit Engine(std::unique_ptr<CodergenBackend> backend);
    Engine(std::unique_ptr<CodergenBackend> backend, EventObserver on_event);

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // Runs the pipeline. Catches all handler exceptions at the boundary.
    // Returns Outcome{success} if the pipeline terminates normally at the exit node
    // with all goal gates satisfied. Returns Outcome::fail(...) otherwise.
    [[nodiscard]] auto run(const Graph& graph, const RunConfig& config) const -> Outcome;

  private:
    std::unique_ptr<CodergenBackend> m_backend;
    HandlerRegistry m_registry;
    EventObserver m_on_event;

    void register_default_handlers(std::unique_ptr<CodergenBackend> backend);

    [[nodiscard]] auto run_from(const Graph& graph, const NodeId& start_id, const RunConfig& config, int loop_depth = 0) const -> Outcome;
};

[[nodiscard]] auto resolve_fidelity(const Node& node, const Edge* incoming_edge,
                                     const Graph& graph) -> FidelityMode;

[[nodiscard]] auto resolve_thread_key(const Node& node, const Edge* incoming_edge,
                                       const Graph& graph,
                                       const NodeId& previous_node_id) -> ThreadId;

}  // namespace attractor

#endif  // ATTRACTOR_ENGINE_HPP
