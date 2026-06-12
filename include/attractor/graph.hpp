#ifndef ATTRACTOR_GRAPH_HPP
#define ATTRACTOR_GRAPH_HPP

#include <attractor/types.hpp>
#include <chrono>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <vector>

namespace attractor {

struct Node {
    NodeId id;
    NodeLabel label;
    NodeShape shape{NodeShape::box};
    HandlerTypeName node_type;
    PromptText prompt;
    std::optional<MaxRetries> max_retries;
    ts::boolean goal_gate{false};
    NodeId retry_target;
    NodeId fallback_retry_target;
    std::optional<FidelityMode> fidelity;
    std::optional<ThreadId> thread_id;
    CssClass css_class;
    std::optional<TimeoutDuration> timeout;
    LlmModel llm_model;
    LlmProvider llm_provider;
    std::optional<ReasoningEffort> reasoning_effort;
    ts::boolean auto_status{false};
    ts::boolean allow_partial{false};
    NodeId human_default_choice;
    ShellCommand tool_command;
    ConditionExpr manager_stop_condition;
    int manager_max_cycles{1000};
    JoinPolicy join_policy{JoinPolicy::wait_all};
    MaxParallel max_parallel{4};
};

struct Edge {
    NodeId from;
    NodeId to;
    EdgeLabel label;
    ConditionExpr condition;
    Weight weight{0};
    std::optional<FidelityMode> fidelity;
    std::optional<ThreadId> thread_id;
    ts::boolean loop_restart{false};
};

struct Graph {
    GraphId digraph_id;
    GoalText goal;
    GraphLabel label;
    StylesheetId model_stylesheet;
    MaxRetries default_max_retries{0};
    std::optional<FidelityMode> default_fidelity;
    NodeId retry_target;
    NodeId fallback_retry_target;
    DotfilePath stack_child_dotfile;
    WorkDir stack_child_workdir;
    ShellCommand tool_hooks_pre;
    ShellCommand tool_hooks_post;
    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

void to_json(nlohmann::json& j, const Node& v);
void from_json(const nlohmann::json& j, Node& v);
void to_json(nlohmann::json& j, const Edge& v);
void from_json(const nlohmann::json& j, Edge& v);
void to_json(nlohmann::json& j, const Graph& v);
void from_json(const nlohmann::json& j, Graph& v);

}  // namespace attractor

#endif  // ATTRACTOR_GRAPH_HPP
