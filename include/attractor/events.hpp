#ifndef ATTRACTOR_EVENTS_HPP
#define ATTRACTOR_EVENTS_HPP

#include <attractor/types.hpp>
#include <functional>
#include <variant>

namespace attractor {

// Emitted before a non-terminal handler executes.
// index: 0-based position in execution order (= completed_nodes.size() before execute).
struct StageStarted {
    NodeId id;
    int index{};
};

// Emitted after a non-terminal handler completes (after retry resolution, before edge selection).
struct StageCompleted {
    NodeId id;
    int index{};
};

using Event = std::variant<StageStarted, StageCompleted>;
using EventObserver = std::function<void(const Event&)>;

}  // namespace attractor

#endif  // ATTRACTOR_EVENTS_HPP
