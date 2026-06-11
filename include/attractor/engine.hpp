#ifndef ATTRACTOR_ENGINE_HPP
#define ATTRACTOR_ENGINE_HPP

#include <attractor/graph.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/types.hpp>

#include <chrono>
#include <functional>

namespace attractor {

enum class BackoffPreset {
    none,                   // 0s delay — immediate retry
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
