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

void to_json(nlohmann::json& j, const Node& n)
{
    j["id"] = nlohmann::json{};
    to_json(j["id"], n.id);
    j["label"] = n.label;
    j["shape"] = n.shape;
    j["node_type"] = nlohmann::json{};
    to_json(j["node_type"], n.node_type);
    j["prompt"] = nlohmann::json{};
    to_json(j["prompt"], n.prompt);
    opt_to_json(j, "max_retries", n.max_retries);
    j["goal_gate"] = n.goal_gate;
    j["retry_target"] = nlohmann::json{};
    to_json(j["retry_target"], n.retry_target);
    j["fallback_retry_target"] = nlohmann::json{};
    to_json(j["fallback_retry_target"], n.fallback_retry_target);
    opt_to_json(j, "fidelity", n.fidelity);
    opt_to_json(j, "thread_id", n.thread_id);
    j["css_class"] = n.css_class;
    opt_to_json(j, "timeout", n.timeout);
    j["llm_model"] = n.llm_model;
    j["llm_provider"] = n.llm_provider;
    j["reasoning_effort"] = n.reasoning_effort;
    j["auto_status"] = n.auto_status;
    j["allow_partial"] = n.allow_partial;
    j["human_default_choice"] = nlohmann::json{};
    to_json(j["human_default_choice"], n.human_default_choice);
}

void from_json(const nlohmann::json& j, Node& n)
{
    from_json(j.at("id"), n.id);
    n.label = j.at("label").get<std::string>();
    n.shape = j.at("shape").get<std::string>();
    from_json(j.at("node_type"), n.node_type);
    from_json(j.at("prompt"), n.prompt);

    // optional<MaxRetries> — constrained_type, no default ctor
    if (j.contains("max_retries") && !j["max_retries"].is_null()) {
        n.max_retries = MaxRetries{j["max_retries"].get<int>()};
    }

    n.goal_gate = j.at("goal_gate").get<bool>();
    from_json(j.at("retry_target"), n.retry_target);
    from_json(j.at("fallback_retry_target"), n.fallback_retry_target);

    opt_fidelity_from_json(j, "fidelity", n.fidelity);

    // optional<ThreadId> — strong_typedef, no default ctor
    if (j.contains("thread_id") && !j["thread_id"].is_null()) {
        n.thread_id = ThreadId{j["thread_id"].get<std::string>()};
    }

    n.css_class = j.at("css_class").get<std::string>();

    // optional<TimeoutDuration> — constrained_type, no default ctor
    if (j.contains("timeout") && !j["timeout"].is_null()) {
        auto ms = std::chrono::milliseconds{j["timeout"].get<int64_t>()};
        if (ms.count() <= 0) {
            throw nlohmann::json::type_error::create(302, "invalid timeout value: must be positive", &j["timeout"]);
        }
        n.timeout = TimeoutDuration{ms};
    }

    n.llm_model = j.at("llm_model").get<std::string>();
    n.llm_provider = j.at("llm_provider").get<std::string>();
    n.reasoning_effort = j.at("reasoning_effort").get<std::string>();
    n.auto_status = j.at("auto_status").get<bool>();
    n.allow_partial = j.at("allow_partial").get<bool>();
    from_json(j.at("human_default_choice"), n.human_default_choice);
}

void to_json(nlohmann::json& j, const Edge& e)
{
    j["from"] = nlohmann::json{};
    to_json(j["from"], e.from);
    j["to"] = nlohmann::json{};
    to_json(j["to"], e.to);
    j["label"] = nlohmann::json{};
    to_json(j["label"], e.label);
    j["condition"] = e.condition;
    j["weight"] = nlohmann::json{};
    to_json(j["weight"], e.weight);
    opt_to_json(j, "fidelity", e.fidelity);
    opt_to_json(j, "thread_id", e.thread_id);
    j["loop_restart"] = e.loop_restart;
}

void from_json(const nlohmann::json& j, Edge& e)
{
    from_json(j.at("from"), e.from);
    from_json(j.at("to"), e.to);
    from_json(j.at("label"), e.label);
    e.condition = j.at("condition").get<std::string>();
    from_json(j.at("weight"), e.weight);
    opt_fidelity_from_json(j, "fidelity", e.fidelity);

    // optional<ThreadId>
    if (j.contains("thread_id") && !j["thread_id"].is_null()) {
        e.thread_id = ThreadId{j["thread_id"].get<std::string>()};
    }

    e.loop_restart = j.at("loop_restart").get<bool>();
}

void to_json(nlohmann::json& j, const Graph& g)
{
    j["digraph_id"] = g.digraph_id;
    j["goal"] = nlohmann::json{};
    to_json(j["goal"], g.goal);
    j["label"] = g.label;
    j["model_stylesheet"] = g.model_stylesheet;
    j["default_max_retries"] = nlohmann::json{};
    to_json(j["default_max_retries"], g.default_max_retries);
    opt_to_json(j, "default_fidelity", g.default_fidelity);
    j["retry_target"] = nlohmann::json{};
    to_json(j["retry_target"], g.retry_target);
    j["fallback_retry_target"] = nlohmann::json{};
    to_json(j["fallback_retry_target"], g.fallback_retry_target);
    j["stack_child_dotfile"] = g.stack_child_dotfile;
    j["stack_child_workdir"] = g.stack_child_workdir;
    j["tool_hooks_pre"] = g.tool_hooks_pre;
    j["tool_hooks_post"] = g.tool_hooks_post;
    j["nodes"] = nlohmann::json::array();
    for (const auto& n : g.nodes) {
        nlohmann::json nj;
        to_json(nj, n);
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
    g.digraph_id = j.at("digraph_id").get<std::string>();
    from_json(j.at("goal"), g.goal);
    g.label = j.at("label").get<std::string>();
    g.model_stylesheet = j.at("model_stylesheet").get<std::string>();
    from_json(j.at("default_max_retries"), g.default_max_retries);
    opt_fidelity_from_json(j, "default_fidelity", g.default_fidelity);
    from_json(j.at("retry_target"), g.retry_target);
    from_json(j.at("fallback_retry_target"), g.fallback_retry_target);
    g.stack_child_dotfile = j.at("stack_child_dotfile").get<std::string>();
    g.stack_child_workdir = j.at("stack_child_workdir").get<std::string>();
    g.tool_hooks_pre = j.at("tool_hooks_pre").get<std::string>();
    g.tool_hooks_post = j.at("tool_hooks_post").get<std::string>();
    for (const auto& nj : j.at("nodes")) {
        Node n;
        from_json(nj, n);
        g.nodes.push_back(n);
    }
    for (const auto& ej : j.at("edges")) {
        Edge e;
        from_json(ej, e);
        g.edges.push_back(e);
    }
}

}  // namespace attractor
