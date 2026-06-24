#ifndef ATTRACTOR_GRAPH_HPP
#define ATTRACTOR_GRAPH_HPP

#include <attractor/types.hpp>
#include <chrono>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace attractor {

struct Node {
    NodeId id;
    NodeLabel label;
    NodeShape shape{NodeShape::box};
    HandlerTypeName node_type;
    std::optional<MaxRetries> max_retries;
    ts::boolean goal_gate{false};
    NodeId retry_target;
    NodeId fallback_retry_target;
    std::optional<FidelityMode> fidelity;
    std::optional<ThreadId> thread_id;
    CssClass css_class;
    std::optional<TimeoutDuration> timeout;
    ts::boolean auto_status{false};
    ts::boolean allow_partial{false};
    std::optional<SubgraphId> enclosing_subgraph;
};

struct StartNode : Node {};
struct ExitNode : Node {};
struct ConditionalNode : Node {};

struct CodergenNode : Node {
    PromptText prompt;
    LlmModel llm_model;
    LlmProvider llm_provider;
    std::optional<ReasoningEffort> reasoning_effort;
};

struct ToolNode : Node {
    ShellCommand tool_command;
};

struct ManagerNode : Node {
    ConditionExpr manager_stop_condition;
    int manager_max_cycles{1000};
};

struct WaitHumanNode : Node {
    NodeId human_default_choice;
};

struct ParallelNode : Node {
    MaxParallel max_parallel{4};
    JoinPolicy join_policy{JoinPolicy::wait_all};
};

struct FanInNode : Node {
    PromptText prompt;
};

using NodeVariant = std::variant<StartNode, ExitNode, CodergenNode, ToolNode, ManagerNode,
                                 WaitHumanNode, ParallelNode, FanInNode, ConditionalNode>;

[[nodiscard]] inline const Node& to_base(const NodeVariant& v) noexcept
{
    return std::visit([](const Node& n) -> const Node& { return n; }, v);
}

[[nodiscard]] inline Node& to_base(NodeVariant& v) noexcept
{
    return std::visit([](Node& n) -> Node& { return n; }, v);
}

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
    std::vector<NodeVariant> nodes;
    std::vector<Edge> edges;
};

void to_json(nlohmann::json& j, const Node& v);
void from_json(const nlohmann::json& j, Node& v);

void to_json(nlohmann::json& j, const StartNode& v);
void from_json(const nlohmann::json& j, StartNode& v);

void to_json(nlohmann::json& j, const ExitNode& v);
void from_json(const nlohmann::json& j, ExitNode& v);

void to_json(nlohmann::json& j, const ConditionalNode& v);
void from_json(const nlohmann::json& j, ConditionalNode& v);

void to_json(nlohmann::json& j, const CodergenNode& v);
void from_json(const nlohmann::json& j, CodergenNode& v);

void to_json(nlohmann::json& j, const ToolNode& v);
void from_json(const nlohmann::json& j, ToolNode& v);

void to_json(nlohmann::json& j, const ManagerNode& v);
void from_json(const nlohmann::json& j, ManagerNode& v);

void to_json(nlohmann::json& j, const WaitHumanNode& v);
void from_json(const nlohmann::json& j, WaitHumanNode& v);

void to_json(nlohmann::json& j, const ParallelNode& v);
void from_json(const nlohmann::json& j, ParallelNode& v);

void to_json(nlohmann::json& j, const FanInNode& v);
void from_json(const nlohmann::json& j, FanInNode& v);

void to_json(nlohmann::json& j, const NodeVariant& v);
void from_json(const nlohmann::json& j, NodeVariant& v);

void to_json(nlohmann::json& j, const Edge& v);
void from_json(const nlohmann::json& j, Edge& v);
void to_json(nlohmann::json& j, const Graph& v);
void from_json(const nlohmann::json& j, Graph& v);

}  // namespace attractor

#endif  // ATTRACTOR_GRAPH_HPP
