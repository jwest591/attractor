#include <attractor/graph.hpp>
#include <nlohmann/json.hpp>

namespace attractor {

// Serialize optional<T> where T has to_json
template<typename T>
static void opt_to_json(nlohmann::json& j, const char* key, const std::optional<T>& v)
{
    if (v) {
        nlohmann::json tmp;
        to_json(tmp, *v);
        j[key] = tmp;
    }
}

// Deserialize optional<FidelityMode> (enum; default-constructible)
static void opt_fidelity_from_json(const nlohmann::json& j, const char* key, std::optional<FidelityMode>& v)
{
    if (j.contains(key) && !j[key].is_null()) {
        FidelityMode fm{};
        from_json(j[key], fm);
        v = fm;
    }
    else {
        v = std::nullopt;
    }
}

// Base Node serialization (common fields only)
void to_json(nlohmann::json& j, const Node& n)
{
    j["id"] = nlohmann::json{};
    to_json(j["id"], n.id);
    j["label"] = nlohmann::json{};
    to_json(j["label"], n.label);
    j["shape"] = nlohmann::json{};
    to_json(j["shape"], n.shape);
    j["node_type"] = nlohmann::json{};
    to_json(j["node_type"], n.node_type);
    opt_to_json(j, "max_retries", n.max_retries);
    j["goal_gate"] = bool{n.goal_gate};
    j["retry_target"] = nlohmann::json{};
    to_json(j["retry_target"], n.retry_target);
    j["fallback_retry_target"] = nlohmann::json{};
    to_json(j["fallback_retry_target"], n.fallback_retry_target);
    opt_to_json(j, "fidelity", n.fidelity);
    opt_to_json(j, "thread_id", n.thread_id);
    j["css_class"] = nlohmann::json{};
    to_json(j["css_class"], n.css_class);
    opt_to_json(j, "timeout", n.timeout);
    j["auto_status"] = bool{n.auto_status};
    j["allow_partial"] = bool{n.allow_partial};
    opt_to_json(j, "enclosing_subgraph", n.enclosing_subgraph);
}

void from_json(const nlohmann::json& j, Node& n)
{
    if (j.contains("id")) { from_json(j.at("id"), n.id); }
    if (j.contains("label")) { from_json(j.at("label"), n.label); }
    if (j.contains("shape")) { from_json(j.at("shape"), n.shape); }
    if (j.contains("node_type")) { from_json(j.at("node_type"), n.node_type); }

    if (j.contains("max_retries") && !j["max_retries"].is_null()) {
        n.max_retries = MaxRetries{j["max_retries"].get<int>()};
    }

    if (j.contains("goal_gate")) { n.goal_gate = j.at("goal_gate").get<bool>(); }
    if (j.contains("retry_target")) { from_json(j.at("retry_target"), n.retry_target); }
    if (j.contains("fallback_retry_target")) { from_json(j.at("fallback_retry_target"), n.fallback_retry_target); }

    opt_fidelity_from_json(j, "fidelity", n.fidelity);

    if (j.contains("thread_id") && !j["thread_id"].is_null()) {
        n.thread_id = ThreadId{j["thread_id"].get<std::string>()};
    }

    if (j.contains("css_class")) { from_json(j.at("css_class"), n.css_class); }

    if (j.contains("timeout") && !j["timeout"].is_null()) {
        auto ms = std::chrono::milliseconds{j["timeout"].get<int64_t>()};
        if (ms.count() <= 0) {
            throw nlohmann::json::type_error::create(302, "invalid timeout value: must be positive", &j["timeout"]);
        }
        n.timeout = TimeoutDuration{ms};
    }

    if (j.contains("auto_status")) { n.auto_status = j.at("auto_status").get<bool>(); }
    if (j.contains("allow_partial")) { n.allow_partial = j.at("allow_partial").get<bool>(); }

    if (j.contains("enclosing_subgraph") && !j["enclosing_subgraph"].is_null()) {
        SubgraphId sg{""};
        from_json(j["enclosing_subgraph"], sg);
        if (!sg.empty()) {
            n.enclosing_subgraph = sg;
        }
    }
}

// Derived node serialization helpers

void to_json(nlohmann::json& j, const StartNode& v)
{
    to_json(j, static_cast<const Node&>(v));
}

void from_json(const nlohmann::json& j, StartNode& v)
{
    from_json(j, static_cast<Node&>(v));
}

void to_json(nlohmann::json& j, const ExitNode& v)
{
    to_json(j, static_cast<const Node&>(v));
}

void from_json(const nlohmann::json& j, ExitNode& v)
{
    from_json(j, static_cast<Node&>(v));
}

void to_json(nlohmann::json& j, const ConditionalNode& v)
{
    to_json(j, static_cast<const Node&>(v));
}

void from_json(const nlohmann::json& j, ConditionalNode& v)
{
    from_json(j, static_cast<Node&>(v));
}

void to_json(nlohmann::json& j, const CodergenNode& v)
{
    to_json(j, static_cast<const Node&>(v));
    j["prompt"] = nlohmann::json{};
    to_json(j["prompt"], v.prompt);
    j["llm_model"] = nlohmann::json{};
    to_json(j["llm_model"], v.llm_model);
    j["llm_provider"] = nlohmann::json{};
    to_json(j["llm_provider"], v.llm_provider);
    opt_to_json(j, "reasoning_effort", v.reasoning_effort);
}

void from_json(const nlohmann::json& j, CodergenNode& v)
{
    from_json(j, static_cast<Node&>(v));
    if (j.contains("prompt")) { from_json(j.at("prompt"), v.prompt); }
    if (j.contains("llm_model")) { from_json(j.at("llm_model"), v.llm_model); }
    if (j.contains("llm_provider")) { from_json(j.at("llm_provider"), v.llm_provider); }
    if (j.contains("reasoning_effort") && !j["reasoning_effort"].is_null()) {
        ReasoningEffort re{};
        from_json(j["reasoning_effort"], re);
        v.reasoning_effort = re;
    }
}

void to_json(nlohmann::json& j, const ToolNode& v)
{
    to_json(j, static_cast<const Node&>(v));
    j["tool_command"] = nlohmann::json{};
    to_json(j["tool_command"], v.tool_command);
}

void from_json(const nlohmann::json& j, ToolNode& v)
{
    from_json(j, static_cast<Node&>(v));
    if (j.contains("tool_command")) { from_json(j.at("tool_command"), v.tool_command); }
}

void to_json(nlohmann::json& j, const ManagerNode& v)
{
    to_json(j, static_cast<const Node&>(v));
    j["manager_stop_condition"] = nlohmann::json{};
    to_json(j["manager_stop_condition"], v.manager_stop_condition);
    j["manager_max_cycles"] = v.manager_max_cycles;
}

void from_json(const nlohmann::json& j, ManagerNode& v)
{
    from_json(j, static_cast<Node&>(v));
    if (j.contains("manager_stop_condition")) {
        from_json(j.at("manager_stop_condition"), v.manager_stop_condition);
    }
    if (j.contains("manager_max_cycles")) {
        const int cycles = j.at("manager_max_cycles").get<int>();
        if (cycles > 0) { v.manager_max_cycles = cycles; }
    }
}

void to_json(nlohmann::json& j, const WaitHumanNode& v)
{
    to_json(j, static_cast<const Node&>(v));
    j["human_default_choice"] = nlohmann::json{};
    to_json(j["human_default_choice"], v.human_default_choice);
}

void from_json(const nlohmann::json& j, WaitHumanNode& v)
{
    from_json(j, static_cast<Node&>(v));
    if (j.contains("human_default_choice")) { from_json(j.at("human_default_choice"), v.human_default_choice); }
}

void to_json(nlohmann::json& j, const ParallelNode& v)
{
    to_json(j, static_cast<const Node&>(v));
    j["max_parallel"] = v.max_parallel;
    j["join_policy"] = v.join_policy;
}

void from_json(const nlohmann::json& j, ParallelNode& v)
{
    from_json(j, static_cast<Node&>(v));
    if (j.contains("max_parallel")) { from_json(j.at("max_parallel"), v.max_parallel); }
    if (j.contains("join_policy")) { from_json(j.at("join_policy"), v.join_policy); }
}

void to_json(nlohmann::json& j, const FanInNode& v)
{
    to_json(j, static_cast<const Node&>(v));
    j["prompt"] = nlohmann::json{};
    to_json(j["prompt"], v.prompt);
}

void from_json(const nlohmann::json& j, FanInNode& v)
{
    from_json(j, static_cast<Node&>(v));
    if (j.contains("prompt")) { from_json(j.at("prompt"), v.prompt); }
}

// NodeVariant serialization

void to_json(nlohmann::json& j, const NodeVariant& v)
{
    std::visit([&j](const auto& derived) { to_json(j, derived); }, v);
}

void from_json(const nlohmann::json& j, NodeVariant& v)
{
    NodeShape shape = NodeShape::box;
    if (j.contains("shape")) { from_json(j["shape"], shape); }
    switch (shape) {
    case NodeShape::mdiamond: { StartNode n; from_json(j, n); v = n; break; }
    case NodeShape::msquare:  { ExitNode  n; from_json(j, n); v = n; break; }
    case NodeShape::box:      { CodergenNode n; from_json(j, n); v = n; break; }
    case NodeShape::parallelogram: { ToolNode n; from_json(j, n); v = n; break; }
    case NodeShape::house:    { ManagerNode n; from_json(j, n); v = n; break; }
    case NodeShape::hexagon:  { WaitHumanNode n; from_json(j, n); v = n; break; }
    case NodeShape::component:      { ParallelNode n; from_json(j, n); v = n; break; }
    case NodeShape::triple_octagon: { FanInNode n; from_json(j, n); v = n; break; }
    case NodeShape::diamond:  { ConditionalNode n; from_json(j, n); v = n; break; }
    }
}

void to_json(nlohmann::json& j, const Edge& e)
{
    j["from"] = nlohmann::json{};
    to_json(j["from"], e.from);
    j["to"] = nlohmann::json{};
    to_json(j["to"], e.to);
    j["label"] = nlohmann::json{};
    to_json(j["label"], e.label);
    j["condition"] = nlohmann::json{};
    to_json(j["condition"], e.condition);
    j["weight"] = nlohmann::json{};
    to_json(j["weight"], e.weight);
    opt_to_json(j, "fidelity", e.fidelity);
    opt_to_json(j, "thread_id", e.thread_id);
    j["loop_restart"] = bool{e.loop_restart};
}

void from_json(const nlohmann::json& j, Edge& e)
{
    from_json(j.at("from"), e.from);
    from_json(j.at("to"), e.to);
    from_json(j.at("label"), e.label);
    from_json(j.at("condition"), e.condition);
    from_json(j.at("weight"), e.weight);
    opt_fidelity_from_json(j, "fidelity", e.fidelity);

    if (j.contains("thread_id") && !j["thread_id"].is_null()) {
        e.thread_id = ThreadId{j["thread_id"].get<std::string>()};
    }

    e.loop_restart = j.at("loop_restart").get<bool>();
}

void to_json(nlohmann::json& j, const Graph& g)
{
    j["digraph_id"] = nlohmann::json{};
    to_json(j["digraph_id"], g.digraph_id);
    j["goal"] = nlohmann::json{};
    to_json(j["goal"], g.goal);
    j["label"] = nlohmann::json{};
    to_json(j["label"], g.label);
    j["model_stylesheet"] = nlohmann::json{};
    to_json(j["model_stylesheet"], g.model_stylesheet);
    j["default_max_retries"] = nlohmann::json{};
    to_json(j["default_max_retries"], g.default_max_retries);
    opt_to_json(j, "default_fidelity", g.default_fidelity);
    j["retry_target"] = nlohmann::json{};
    to_json(j["retry_target"], g.retry_target);
    j["fallback_retry_target"] = nlohmann::json{};
    to_json(j["fallback_retry_target"], g.fallback_retry_target);
    j["stack_child_dotfile"] = nlohmann::json{};
    to_json(j["stack_child_dotfile"], g.stack_child_dotfile);
    j["stack_child_workdir"] = nlohmann::json{};
    to_json(j["stack_child_workdir"], g.stack_child_workdir);
    j["tool_hooks_pre"] = nlohmann::json{};
    to_json(j["tool_hooks_pre"], g.tool_hooks_pre);
    j["tool_hooks_post"] = nlohmann::json{};
    to_json(j["tool_hooks_post"], g.tool_hooks_post);
    j["nodes"] = nlohmann::json::array();
    for (const auto& nv : g.nodes) {
        nlohmann::json nj;
        to_json(nj, nv);
        j["nodes"].push_back(nj);
    }
    j["edges"] = nlohmann::json::array();
    for (const auto& e : g.edges) {
        nlohmann::json ej;
        to_json(ej, e);
        j["edges"].push_back(ej);
    }
}

void from_json(const nlohmann::json& j, Graph& g)
{
    from_json(j.at("digraph_id"), g.digraph_id);
    from_json(j.at("goal"), g.goal);
    from_json(j.at("label"), g.label);
    from_json(j.at("model_stylesheet"), g.model_stylesheet);
    from_json(j.at("default_max_retries"), g.default_max_retries);
    opt_fidelity_from_json(j, "default_fidelity", g.default_fidelity);
    from_json(j.at("retry_target"), g.retry_target);
    from_json(j.at("fallback_retry_target"), g.fallback_retry_target);
    from_json(j.at("stack_child_dotfile"), g.stack_child_dotfile);
    from_json(j.at("stack_child_workdir"), g.stack_child_workdir);
    from_json(j.at("tool_hooks_pre"), g.tool_hooks_pre);
    from_json(j.at("tool_hooks_post"), g.tool_hooks_post);
    for (const auto& nj : j.at("nodes")) {
        NodeVariant nv;
        from_json(nj, nv);
        g.nodes.push_back(nv);
    }
    for (const auto& ej : j.at("edges")) {
        Edge e;
        from_json(ej, e);
        g.edges.push_back(e);
    }
}

}  // namespace attractor
