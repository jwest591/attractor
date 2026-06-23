#ifndef ATTRACTOR_RUN_CONFIG_HPP
#define ATTRACTOR_RUN_CONFIG_HPP

#include <attractor/types.hpp>

#include <chrono>
#include <functional>

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
    bool resume{false};      // If true, load checkpoint.json from logs_root to restore state
    int max_loop_depth{1000}; // Cap on loop_restart recursion depth; exceeding it returns FAIL
};

}  // namespace attractor

#endif  // ATTRACTOR_RUN_CONFIG_HPP
