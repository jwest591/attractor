#include <attractor/engine.hpp>

#include <algorithm>
#include <attractor/backends/noop_backend.hpp>
#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/handlers/codergen_handler.hpp>
#include <attractor/handlers/conditional_handler.hpp>
#include <attractor/handlers/exit_handler.hpp>
#include <attractor/handlers/start_handler.hpp>
#include <attractor/types.hpp>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_safe/strong_typedef.hpp>
#include <vector>

namespace attractor {

namespace {

const Node* find_start_node(const Graph& graph)
{
    for (const auto& node : graph.nodes) {
        const auto& s = type_safe::get(node.shape);
        const auto& id = type_safe::get(node.id);
        if (s == "Mdiamond" || id == "start" || id == "Start") {
            return &node;
        }
    }
    return nullptr;
}

bool is_terminal(const Node& node) noexcept
{
    const auto& s = type_safe::get(node.shape);
    const auto& id = type_safe::get(node.id);
    return s == "Msquare" || id == "exit" || id == "end";
}

const Node* find_node(const Graph& graph, const NodeId& id)
{
    for (const auto& node : graph.nodes) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

Outcome safe_execute(const Handler& handler, const Node& node, Context& ctx, const Graph& graph,
                     const LogsRoot& logs_root)
{
    try {
        return handler.execute(node, ctx, graph, logs_root);
    }
    catch (const std::exception& ex) {
        return Outcome::fail(DiagnosticMessage{ex.what()});
    }
    catch (...) {
        return Outcome::fail(DiagnosticMessage{"handler threw unknown exception"});
    }
}

// Edge selection Steps 4+5 for unconditional edges (condition is empty).
// Steps 1-3 (condition evaluation, preferred_label, suggested_next_ids) deferred to Story 2.4.
const Edge* select_edge(const Node& node, const Graph& graph)
{
    std::vector<const Edge*> candidates;
    for (const auto& edge : graph.edges) {
        if (edge.from == node.id && edge.condition.empty()) {
            candidates.push_back(&edge);
        }
    }
    if (candidates.empty()) {
        return nullptr;
    }

    std::sort(candidates.begin(), candidates.end(), [](const Edge* a, const Edge* b) {
        const int wa = a->weight.get_value();
        const int wb = b->weight.get_value();
        if (wa != wb) {
            return wa > wb;
        }
        return a->to < b->to;
    });

    return candidates[0];
}

}  // namespace

Engine::Engine()
{
    auto noop = std::make_shared<NoOpBackend>();
    m_registry.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    m_registry.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    m_registry.register_handler(HandlerTypeName{"codergen"}, std::make_unique<CodergenHandler>(noop));
    m_registry.register_handler(HandlerTypeName{"conditional"}, std::make_unique<ConditionalHandler>());
    m_registry.set_default_handler(std::make_unique<StartHandler>());
}

Engine::Engine(HandlerRegistry registry) : m_registry{std::move(registry)} {}

auto Engine::run(const Graph& graph, const RunConfig& config) const -> Outcome
{
    const Node* start = find_start_node(graph);
    if (start == nullptr) {
        return Outcome::fail(DiagnosticMessage{"No start node found in graph"});
    }
    return run_from(graph, start->id, config);
}

auto Engine::run_from(const Graph& graph, const NodeId& start_id, const RunConfig& config) const -> Outcome
{
    std::map<NodeId, Outcome> node_outcomes;
    NodeId current_id = start_id;

    while (true) {
        const Node* node = find_node(graph, current_id);
        if (node == nullptr) {
            return Outcome::fail(DiagnosticMessage{"Node not found: " + type_safe::get(current_id)});
        }

        if (is_terminal(*node)) {
            bool retry_requested = false;
            for (const auto& [node_id, outcome] : node_outcomes) {
                const Node* gn = find_node(graph, node_id);
                if (gn != nullptr && static_cast<bool>(gn->goal_gate)) {
                    if (outcome.status != StageStatus::success && outcome.status != StageStatus::partial_success) {
                        const NodeId* rt = nullptr;
                        if (!gn->retry_target.empty()) {
                            rt = &gn->retry_target;
                        }
                        else if (!gn->fallback_retry_target.empty()) {
                            rt = &gn->fallback_retry_target;
                        }
                        else if (!graph.retry_target.empty()) {
                            rt = &graph.retry_target;
                        }
                        else if (!graph.fallback_retry_target.empty()) {
                            rt = &graph.fallback_retry_target;
                        }

                        if (rt != nullptr) {
                            current_id = *rt;
                            retry_requested = true;
                            break;
                        }
                        return Outcome::fail(DiagnosticMessage{"Goal gate unsatisfied: " + type_safe::get(node_id)});
                    }
                }
            }
            if (retry_requested) {
                continue;
            }
            return Outcome{};
        }

        Context ctx;
        const Handler& handler = m_registry.resolve(*node);
        const Outcome outcome = safe_execute(handler, *node, ctx, graph, config.logs_root);

        node_outcomes[node->id] = outcome;

        const Edge* next_edge = select_edge(*node, graph);

        if (next_edge == nullptr) {
            if (outcome.status == StageStatus::fail || outcome.status == StageStatus::retry) {
                return outcome;
            }
            return Outcome{};
        }

        if (static_cast<bool>(next_edge->loop_restart)) {
            return run_from(graph, next_edge->to, config);
        }

        if (outcome.status == StageStatus::fail || outcome.status == StageStatus::retry) {
            return outcome;
        }

        current_id = next_edge->to;
    }
}

}  // namespace attractor
