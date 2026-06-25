#include <attractor/engine.hpp>

#include <algorithm>
#include <attractor/backends/noop_backend.hpp>
#include <variant>
#include <attractor/checkpoint.hpp>
#include <attractor/condition_eval.hpp>
#include <attractor/context.hpp>
#include <attractor/events.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/handlers/codergen_handler.hpp>
#include <attractor/handlers/conditional_handler.hpp>
#include <attractor/handlers/exit_handler.hpp>
#include <attractor/handlers/fan_in_handler.hpp>
#include <attractor/handlers/parallel_handler.hpp>
#include <attractor/handlers/start_handler.hpp>
#include <attractor/handlers/tool_handler.hpp>
#include <attractor/handlers/wait_for_human_handler.hpp>
#include <attractor/interviewer.hpp>
#include <attractor/types.hpp>
#include <cctype>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_safe/strong_typedef.hpp>
#include <vector>

namespace attractor {

namespace {

const NodeVariant* find_start_node(const Graph& graph)
{
    for (const auto& nv : graph.nodes) {
        const Node& node = to_base(nv);
        if (node.shape == NodeShape::mdiamond || node.id == "start" || node.id == "Start") {
            return &nv;
        }
    }
    return nullptr;
}

bool is_terminal(const Node& node) noexcept
{
    return node.shape == NodeShape::msquare || node.id == "exit" || node.id == "end";
}

const NodeVariant* find_node(const Graph& graph, const NodeId& id)
{
    for (const auto& nv : graph.nodes) {
        if (to_base(nv).id == id) {
            return &nv;
        }
    }
    return nullptr;
}

Outcome safe_execute(const Handler& handler, const Node& node, Context& ctx, const Graph& graph,
                     const RunConfig& run_config)
{
    try {
        return handler.execute(node, ctx, graph, run_config);
    }
    catch (const std::exception& ex) {
        return Outcome::fail(DiagnosticMessage{ex.what()});
    }
    catch (...) {
        return Outcome::fail(DiagnosticMessage{"handler threw unknown exception"});
    }
}

std::string stage_status_to_string(StageStatus status)
{
    switch (status) {
    case StageStatus::success:
        return "success";
    case StageStatus::partial_success:
        return "partial_success";
    case StageStatus::fail:
        return "fail";
    case StageStatus::retry:
        return "retry";
    case StageStatus::skipped:
        return "skipped";
    }
    return "";
}

std::string trim(const std::string& s)
{
    const auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

EdgeLabel normalize_label(const EdgeLabel& s)
{
    std::string t = trim(type_safe::get(s));

    if (t.size() >= 4 && t[0] == '[') {
        if (t[2] == ']' && t[3] == ' ') {
            t = t.substr(4);
        }
    }
    else if (t.size() >= 3 && std::isalnum(static_cast<unsigned char>(t[0])) && t[1] == ')' && t[2] == ' ') {
        t = t.substr(3);
    }
    else if (t.size() >= 4 && std::isalnum(static_cast<unsigned char>(t[0])) && t[1] == ' ' && t[2] == '-' &&
             t[3] == ' ') {
        t = t.substr(4);
    }

    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return EdgeLabel(t);
}

std::string lookup_context(const nlohmann::json& snapshot, std::string_view path)
{
    const nlohmann::json* current = &snapshot;

    while (!path.empty()) {
        const auto dot = path.find('.');
        const std::string key(dot == std::string_view::npos ? path : path.substr(0, dot));

        if (!current->is_object() || !current->contains(key)) {
            return "";
        }
        current = &(*current)[key];

        if (dot == std::string_view::npos) {
            break;
        }
        path = path.substr(dot + 1);
    }

    if (current->is_string()) {
        return current->get<std::string>();
    }
    if (current->is_null()) {
        return "";
    }
    return current->dump();
}

bool eval_clause(const std::string& clause_str, const Outcome& outcome, const nlohmann::json& context_snapshot)
{
    const std::string clause = trim(clause_str);

    const auto neq_pos = clause.find("!=");
    const auto eq_pos = clause.find('=');

    std::string key_str;
    std::string op;
    std::string val;

    if (neq_pos != std::string::npos && (eq_pos == std::string::npos || neq_pos < eq_pos)) {
        key_str = trim(clause.substr(0, neq_pos));
        op = "!=";
        val = trim(clause.substr(neq_pos + 2));
    }
    else if (eq_pos != std::string::npos) {
        key_str = trim(clause.substr(0, eq_pos));
        op = "=";
        val = trim(clause.substr(eq_pos + 1));
    }
    else {
        return false;
    }

    std::string actual;
    if (key_str == "outcome") {
        actual = stage_status_to_string(outcome.status);
    }
    else if (key_str == "preferred_label") {
        actual = type_safe::get(outcome.preferred_label);
    }
    else if (key_str.starts_with("context.")) {
        actual = lookup_context(context_snapshot, std::string_view(key_str).substr(8));
    }
    else {
        return false;
    }

    if (op == "=") {
        return actual == val;
    }
    if (op == "!=") {
        return actual != val;
    }
    return false;
}

const Edge* best_by_weight_then_lexical(std::vector<const Edge*>& candidates)
{
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

// Returns base delay for attempt n (0-based), before jitter.
std::chrono::duration<double> compute_backoff_delay(BackoffPreset preset, int attempt)
{
    using namespace std::chrono_literals;
    switch (preset) {
    case BackoffPreset::none:
        return 0s;
    case BackoffPreset::fixed_1s:
        return 1.0s;
    case BackoffPreset::exponential_100ms: {
        const double d = 0.1 * (1 << std::min(attempt, 7));
        return std::chrono::duration<double>(std::min(d, 10.0));
    }
    case BackoffPreset::exponential_1s: {
        const double d = 1.0 * (1 << std::min(attempt, 6));
        return std::chrono::duration<double>(std::min(d, 60.0));
    }
    case BackoffPreset::exponential_jitter_1s: {
        const double base = 1.0 * (1 << std::min(attempt, 5));
        const double capped = std::min(base, 60.0);
        const double jitter = (attempt % 2 == 0) ? 0.75 : 1.25;
        return std::chrono::duration<double>(capped * jitter);
    }
    default:
        return {};
    }
}

void do_sleep(const RetryPolicy& policy, int attempt)
{
    const auto delay = compute_backoff_delay(policy.preset, attempt);
    if (delay.count() <= 0.0) {
        return;
    }
    if (policy.sleep_fn) {
        policy.sleep_fn(delay);
    }
    else {
        std::this_thread::sleep_for(delay);
    }
}

int effective_max_retries(const Node& node, const Graph& graph)
{
    if (node.max_retries.has_value()) {
        return node.max_retries->get_value();
    }
    return graph.default_max_retries.get_value();
}

// 5-step deterministic edge selection.
// Step 1: condition match; Steps 2-5: unconditional edges only.
// Returns nullptr if no edge is selectable.
const Edge* select_edge(const Node& node, const Outcome& outcome, const nlohmann::json& context_snapshot,
                        const Graph& graph)
{
    std::vector<const Edge*> conditional_candidates;
    std::vector<const Edge*> unconditional_candidates;

    for (const auto& edge : graph.edges) {
        if (edge.from != node.id) {
            continue;
        }
        if (edge.condition.empty()) {
            unconditional_candidates.push_back(&edge);
        }
        else if (eval_condition(edge.condition, outcome, context_snapshot)) {
            conditional_candidates.push_back(&edge);
        }
    }

    // Step 1: any condition matched?
    if (!conditional_candidates.empty()) {
        return best_by_weight_then_lexical(conditional_candidates);
    }

    if (unconditional_candidates.empty()) {
        return nullptr;
    }

    // Step 2: preferred_label match (first match among unconditional edges)
    if (!outcome.preferred_label.empty()) {
        const auto norm_pref = normalize_label(outcome.preferred_label);
        for (const auto* edge : unconditional_candidates) {
            if (!edge->label.empty() && normalize_label(edge->label) == norm_pref) {
                return edge;
            }
        }
    }

    // Step 3: suggested_next_ids match (first id in list order, first matching edge)
    if (!outcome.suggested_next_ids.empty()) {
        for (const NodeId& nid : outcome.suggested_next_ids) {
            for (const auto* edge : unconditional_candidates) {
                if (edge->to == nid) {
                    return edge;
                }
            }
        }
    }

    // Steps 4+5: weight descending, lexical tiebreak on target node ID
    return best_by_weight_then_lexical(unconditional_candidates);
}

}  // namespace

bool eval_condition(const ConditionExpr& condition, const Outcome& outcome, const nlohmann::json& context_snapshot)
{
    if (condition.empty()) {
        return true;
    }

    const std::string expr = type_safe::get(condition);

    std::string::size_type pos = 0;
    while (true) {
        const auto amp = expr.find("&&", pos);
        const std::string clause = (amp == std::string::npos) ? expr.substr(pos) : expr.substr(pos, amp - pos);

        if (!eval_clause(clause, outcome, context_snapshot)) {
            return false;
        }

        if (amp == std::string::npos) {
            break;
        }
        pos = amp + 2;
    }
    return true;
}

auto resolve_fidelity(const Node& node, const Edge* incoming_edge, const Graph& graph) -> FidelityMode
{
    if (incoming_edge && incoming_edge->fidelity.has_value()) {
        return *incoming_edge->fidelity;
    }
    if (node.fidelity.has_value()) {
        return *node.fidelity;
    }
    if (graph.default_fidelity.has_value()) {
        return *graph.default_fidelity;
    }
    return FidelityMode::compact;
}

auto resolve_thread_key(const Node& node, const Edge* incoming_edge, const Graph& graph) -> ThreadId
{
    (void)graph;
    if (node.thread_id.has_value()) {
        return *node.thread_id;
    }
    if (incoming_edge && incoming_edge->thread_id.has_value()) {
        return *incoming_edge->thread_id;
    }
    return ThreadId{type_safe::get(node.id)};
}

void Engine::register_default_handlers(std::unique_ptr<CodergenBackend> backend)
{
    m_backend = std::move(backend);
    m_registry.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    m_registry.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    m_registry.register_handler(HandlerTypeName{"codergen"}, std::make_unique<CodergenHandler>(m_backend.get()));
    m_registry.register_handler(HandlerTypeName{"conditional"}, std::make_unique<ConditionalHandler>());
    static AutoApproveInterviewer k_default_interviewer;
    m_registry.register_handler(HandlerTypeName{"wait.human"},
                                std::make_unique<WaitForHumanHandler>(k_default_interviewer));
    m_registry.register_handler(
        HandlerTypeName{"parallel"},
        std::make_unique<ParallelHandler>([this](const Graph& g, const NodeId& id, const RunConfig& cfg) -> Outcome {
            return run_from(g, id, cfg);
        }));
    m_registry.register_handler(HandlerTypeName{"parallel.fan_in"}, std::make_unique<FanInHandler>(m_backend.get()));
    m_registry.register_handler(HandlerTypeName{"tool"}, std::make_unique<ToolHandler>());
    m_registry.set_default_handler(std::make_unique<StartHandler>());
}

Engine::Engine() { register_default_handlers(std::make_unique<NoOpBackend>()); }

Engine::Engine(HandlerRegistry registry) : m_registry{std::move(registry)} {}

Engine::Engine(HandlerRegistry registry, EventObserver on_event)
    : m_registry{std::move(registry)}
    , m_on_event{std::move(on_event)}
{
}

Engine::Engine(EventObserver on_event) : Engine() { m_on_event = std::move(on_event); }

Engine::Engine(std::unique_ptr<CodergenBackend> backend) { register_default_handlers(std::move(backend)); }

Engine::Engine(std::unique_ptr<CodergenBackend> backend, EventObserver on_event) : Engine(std::move(backend))
{
    m_on_event = std::move(on_event);
}

auto Engine::run(const Graph& graph, const RunConfig& config) const -> Outcome
{
    const NodeVariant* start = find_start_node(graph);
    if (start == nullptr) {
        return Outcome::fail(DiagnosticMessage{"No start node found in graph"});
    }
    return run_from(graph, to_base(*start).id, config);
}

auto Engine::run_from(const Graph& graph, const NodeId& start_id, const RunConfig& config, int loop_depth) const -> Outcome
{
    if (loop_depth > config.max_loop_depth) {
        return Outcome::fail(DiagnosticMessage{"loop_restart depth limit exceeded"});
    }

    std::map<NodeId, Outcome> node_outcomes;
    NodeId current_id = start_id;
    Context ctx;
    std::vector<NodeId> completed_nodes;
    std::map<std::string, int> node_retries_map;
    int terminal_retry_count = 0;

    // Resume: restore context and starting point from checkpoint.
    if (config.resume) {
        if (config.logs_root.empty()) {
            return Outcome::fail(DiagnosticMessage{"resume requires a non-empty logs_root"});
        }
        auto cp = load_checkpoint(config.logs_root);
        if (!cp) {
            return Outcome::fail(DiagnosticMessage{"resume failed: " + cp.error()});
        }
        ctx.merge_updates(cp->context);
        completed_nodes = cp->completed_nodes;
        node_retries_map = cp->node_retries;
        if (!cp->current_node.empty()) {
            current_id = cp->current_node;
        }
        ctx.set_execution_counter(cp->execution_counter);
    }

    while (true) {
        const NodeVariant* node_var = find_node(graph, current_id);
        if (node_var == nullptr) {
            return Outcome::fail(DiagnosticMessage{"Node not found: " + type_safe::get(current_id)});
        }
        const Node& node = to_base(*node_var);

        if (is_terminal(node)) {
            // Collect all unsatisfied goal gates in a single pass.
            // skipped counts as unsatisfied: engine retries or fails, never proceeds as satisfied.
            struct GateInfo {
                NodeId gate_id;
                const NodeId* retry_target;
            };
            std::vector<GateInfo> unsatisfied;

            for (const auto& [node_id, gate_outcome] : node_outcomes) {
                const NodeVariant* gv = find_node(graph, node_id);
                if (gv == nullptr) { continue; }
                const Node& gn = to_base(*gv);
                if (!static_cast<bool>(gn.goal_gate)) {
                    continue;
                }
                if (gate_outcome.status == StageStatus::success ||
                    gate_outcome.status == StageStatus::partial_success) {
                    continue;
                }
                const NodeId* rt = nullptr;
                if (!gn.retry_target.empty())                  { rt = &gn.retry_target; }
                else if (!gn.fallback_retry_target.empty())    { rt = &gn.fallback_retry_target; }
                else if (!graph.retry_target.empty())           { rt = &graph.retry_target; }
                else if (!graph.fallback_retry_target.empty())  { rt = &graph.fallback_retry_target; }
                unsatisfied.push_back({node_id, rt});
            }

            if (!unsatisfied.empty()) {
                // Any gate with no retry_target fails the pipeline immediately.
                for (const auto& gi : unsatisfied) {
                    if (gi.retry_target == nullptr) {
                        return Outcome::fail(
                            DiagnosticMessage{"Goal gate unsatisfied: " + type_safe::get(gi.gate_id)});
                    }
                }
                // Divergent retry targets are ambiguous — fail rather than silently pick one.
                const NodeId* first_target = unsatisfied.front().retry_target;
                for (const auto& gi : unsatisfied) {
                    if (*gi.retry_target != *first_target) {
                        return Outcome::fail(DiagnosticMessage{
                            "Goal gates have conflicting retry targets"});
                    }
                }
                // Iteration cap prevents unbounded retry cycles (matches loop_restart cap).
                if (terminal_retry_count > config.max_loop_depth) {
                    return Outcome::fail(DiagnosticMessage{"Goal gate retry limit exceeded"});
                }
                ++terminal_retry_count;
                // Copy retry target before erasing gate outcomes.
                const NodeId retry_from = *first_target;
                for (const auto& gi : unsatisfied) {
                    node_outcomes.erase(gi.gate_id);
                }
                current_id = retry_from;
                continue;
            }

            for (const auto& [nid, nout] : node_outcomes) {
                if (nout.status == StageStatus::partial_success) {
                    const NodeVariant* nv = find_node(graph, nid);
                    if (nv != nullptr && static_cast<bool>(to_base(*nv).allow_partial)) {
                        return nout;
                    }
                }
            }
            return Outcome{};
        }

        // 0-based index = count of already-completed nodes before this one.
        const int node_index = static_cast<int>(completed_nodes.size());

        (void)ctx.next_execution_counter();

        if (m_on_event) {
            m_on_event(Event{
                StageStarted{node.id, node_index}
            });
        }

        Outcome outcome;
        std::visit([&](const auto& derived_node) {
            const Handler& handler = m_registry.resolve(derived_node);
            const int max_retries = effective_max_retries(node, graph);
            int remaining = max_retries;
            int attempt = 0;

            do {
                outcome = safe_execute(handler, derived_node, ctx, graph, config);
                node_outcomes[node.id] = outcome;
                if (outcome.status != StageStatus::retry) {
                    break;
                }
                if (remaining == 0) {
                    break;
                }
                --remaining;
                do_sleep(config.retry_policy, attempt);
                ++attempt;
            } while (true);

            if (attempt > 0) {
                node_retries_map[type_safe::get(node.id)] = attempt;
            }
        }, *node_var);

        if (outcome.status == StageStatus::retry) {
            if (static_cast<bool>(node.allow_partial)) {
                outcome = Outcome{.status = StageStatus::partial_success,
                                  .failure_reason =
                                      DiagnosticMessage{"Retry exhausted (partial): " + type_safe::get(node.id)}};
            }
            else {
                outcome = Outcome::fail(DiagnosticMessage{"Retry exhausted: " + type_safe::get(node.id)});
            }
            node_outcomes[node.id] = outcome;
        }

        // Emit StageCompleted after final outcome is determined.
        if (m_on_event) {
            m_on_event(Event{
                StageCompleted{node.id, node_index}
            });
        }

        // Merge context_updates into persistent context; set engine-controlled keys.
        ctx.merge_updates(outcome.context_updates);
        (void)ctx.set(ContextKey{"outcome"}, stage_status_to_string(outcome.status));
        if (!outcome.preferred_label.empty()) {
            (void)ctx.set(ContextKey{"preferred_label"}, type_safe::get(outcome.preferred_label));
        }

        // Use context snapshot for edge condition evaluation.
        const auto context_snapshot = ctx.snapshot();
        const Edge* next_edge = select_edge(node, outcome, context_snapshot, graph);

        if (next_edge == nullptr) {
            if (outcome.status == StageStatus::fail || outcome.status == StageStatus::retry) {
                return outcome;
            }
            // skipped with no outgoing edges: treat as partial_success if allow_partial, else FAIL.
            // Implicit success for skipped would silently mask unintended termination.
            if (outcome.status == StageStatus::skipped) {
                if (static_cast<bool>(node.allow_partial)) {
                    return Outcome{.status = StageStatus::partial_success,
                                   .failure_reason =
                                       DiagnosticMessage{"skipped node: " + type_safe::get(node.id)}};
                }
                return Outcome::fail(
                    DiagnosticMessage{"skipped node with no outgoing edges: " + type_safe::get(node.id)});
            }
            return Outcome{};
        }

        if (static_cast<bool>(next_edge->loop_restart)) {
            RunConfig fresh_config = config;
            fresh_config.resume = false;
            return run_from(graph, next_edge->to, fresh_config, loop_depth + 1);
        }

        // Fail/retry outcomes do not follow unconditional edges, except when goal_gate is
        // set -- the terminal node must evaluate the gate before deciding to retry or fail.
        if ((outcome.status == StageStatus::fail || outcome.status == StageStatus::retry) &&
            next_edge->condition.empty() && !static_cast<bool>(node.goal_gate)) {
            return outcome;
        }

        completed_nodes.push_back(node.id);
        current_id = next_edge->to;

        // Checkpoint saved AFTER advancing current_id; current_node = NEXT node to execute.
        if (!config.logs_root.empty()) {
            CheckpointData cp;
            cp.current_node = current_id;
            cp.completed_nodes = completed_nodes;
            cp.context = context_snapshot;
            cp.node_retries = node_retries_map;
            cp.execution_counter = ctx.current_execution_counter();
            (void)save_checkpoint(config.logs_root, cp);
        }
    }
}

}  // namespace attractor
