#ifndef ATTRACTOR_ENGINE_HPP
#define ATTRACTOR_ENGINE_HPP

#include <attractor/events.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/types.hpp>

#include <chrono>
#include <functional>
#include <memory>

namespace attractor {

enum class BackoffPreset {
    none,                   // 0s delay -- immediate retry
    fixed_1s,               // constant 1 000ms
    exponential_100ms,      // 100ms * 2^attempt, capped at 10s, no jitter
    exponential_1s,         // 1s * 2^attempt, capped at 60s, no jitter
    exponential_jitter_1s,  // 1s * 2^attempt, capped at 60s, deterministic pseudo-jitter +-25%
};

struct RetryPolicy {
    BackoffPreset preset{BackoffPreset::none};
    // Injected sleep; null => std::this_thread::sleep_for (ASR-1)
    std::function<void(std::chrono::duration<double>)> sleep_fn;
};

struct RunConfig {
    LogsRoot logs_root;
    RetryPolicy retry_policy{};
    bool resume{false};  // If true, load checkpoint.json from logs_root to restore state
};

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
    explicit Engine(std::shared_ptr<CodergenBackend> backend);
    Engine(std::shared_ptr<CodergenBackend> backend, EventObserver on_event);

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // Runs the pipeline. Catches all handler exceptions at the boundary.
    // Returns Outcome{success} if the pipeline terminates normally at the exit node
    // with all goal gates satisfied. Returns Outcome::fail(...) otherwise.
    [[nodiscard]] auto run(const Graph& graph, const RunConfig& config) const -> Outcome;

  private:
    HandlerRegistry m_registry;
    EventObserver m_on_event;

    void register_default_handlers(std::shared_ptr<CodergenBackend> backend);

    [[nodiscard]] auto run_from(const Graph& graph, const NodeId& start_id, const RunConfig& config) const -> Outcome;
};

[[nodiscard]] auto resolve_fidelity(const Node& node, const Edge* incoming_edge,
                                     const Graph& graph) -> FidelityMode;

[[nodiscard]] auto resolve_thread_key(const Node& node, const Edge* incoming_edge,
                                       const Graph& graph) -> ThreadId;

}  // namespace attractor

#endif  // ATTRACTOR_ENGINE_HPP
