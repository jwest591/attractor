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
    std::string label;
    std::string shape{"box"};
    HandlerTypeName node_type;
    PromptText prompt;
    std::optional<MaxRetries> max_retries;
    bool goal_gate{false};
    NodeId retry_target;
    NodeId fallback_retry_target;
    std::optional<FidelityMode> fidelity;
    std::optional<ThreadId> thread_id;
    std::string css_class;
    std::optional<TimeoutDuration> timeout;
    std::string llm_model;
    std::string llm_provider;
    std::string reasoning_effort;
    bool auto_status{false};
    bool allow_partial{false};
    NodeId human_default_choice;
};

struct Edge {
    NodeId from;
    NodeId to;
    EdgeLabel label;
    std::string condition;
    Weight weight{0};
    std::optional<FidelityMode> fidelity;
    std::optional<ThreadId> thread_id;
    bool loop_restart{false};
};

struct Graph {
    std::string digraph_id;
    GoalText goal;
    std::string label;
    std::string model_stylesheet;
    MaxRetries default_max_retries{0};
    std::optional<FidelityMode> default_fidelity;
    NodeId retry_target;
    NodeId fallback_retry_target;
    std::string stack_child_dotfile;
    std::string stack_child_workdir;
    std::string tool_hooks_pre;
    std::string tool_hooks_post;
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
