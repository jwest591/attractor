#include <attractor/validator.hpp>

#include <attractor/types.hpp>

#include <algorithm>
#include <queue>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace attractor {

ValidationError::ValidationError(std::vector<Diagnostic> errors)
    : std::runtime_error([&] {
        std::ostringstream oss;
        oss << "Validation failed: " << errors.size() << " error(s)";
        return oss.str();
    }())
    , m_diagnostics{std::move(errors)}
{
}

const std::vector<Diagnostic>& ValidationError::diagnostics() const noexcept { return m_diagnostics; }

static auto is_start_node(const Node& n) -> bool
{
    const auto& id = type_safe::get(n.id);
    return n.shape == NodeShape::mdiamond || id == "start" || id == "Start";
}

static auto is_terminal_node(const Node& n) -> bool
{
    const auto& id = type_safe::get(n.id);
    return n.shape == NodeShape::msquare || id == "exit" || id == "end";
}

static auto reachable_ids(const Graph& g, const NodeId& start_id) -> std::unordered_set<std::string>
{
    std::unordered_set<std::string> visited;
    std::queue<std::string> q;
    const auto& sid = type_safe::get(start_id);
    q.push(sid);
    visited.insert(sid);

    std::unordered_map<std::string, std::vector<std::string>> adj;
    for (const auto& e : g.edges) {
        adj[type_safe::get(e.from)].push_back(type_safe::get(e.to));
    }

    while (!q.empty()) {
        auto cur = q.front();
        q.pop();
        if (auto it = adj.find(cur); it != adj.end()) {
            for (const auto& nxt : it->second) {
                if (visited.insert(nxt).second) {
                    q.push(nxt);
                }
            }
        }
    }
    return visited;
}

// Expected shape for each built-in handler type. Returns nullopt for custom types.
static auto shape_for_handler_type(std::string_view nt) -> std::optional<NodeShape>
{
    if (nt == "codergen")     return NodeShape::box;
    if (nt == "tool")         return NodeShape::parallelogram;
    if (nt == "manager.loop") return NodeShape::house;
    if (nt == "wait.human")   return NodeShape::hexagon;
    if (nt == "parallel")     return NodeShape::component;
    if (nt == "fan.in")       return NodeShape::triple_octagon;
    if (nt == "conditional")  return NodeShape::diamond;
    if (nt == "start")        return NodeShape::mdiamond;
    if (nt == "exit")         return NodeShape::msquare;
    return std::nullopt;
}

static auto trim(std::string_view s) -> std::string
{
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string_view::npos) {
        return {};
    }
    auto e = s.find_last_not_of(" \t\r\n");
    return std::string{s.substr(b, e - b + 1)};
}

static auto is_valid_condition_key(std::string_view key) -> bool
{
    if (key == "outcome" || key == "preferred_label") {
        return true;
    }
    if (key.starts_with("context.") && key.size() > 8) {
        return true;
    }
    return false;
}

static auto parse_condition_syntax(std::string_view condition) -> bool
{
    if (condition.empty()) {
        return true;
    }

    std::string s{condition};
    std::vector<std::string> clauses;
    size_t pos = 0;
    while (pos <= s.size()) {
        auto amp = s.find("&&", pos);
        if (amp == std::string::npos) {
            clauses.push_back(trim(s.substr(pos)));
            break;
        }
        clauses.push_back(trim(s.substr(pos, amp - pos)));
        pos = amp + 2;
    }

    for (const auto& clause : clauses) {
        if (clause.empty()) {
            return false;
        }

        auto neq = clause.find("!=");
        auto eq = clause.find('=');

        if (neq != std::string::npos && (eq == std::string::npos || neq < eq)) {
            auto key = trim(clause.substr(0, neq));
            auto literal = trim(clause.substr(neq + 2));
            if (!is_valid_condition_key(key) || literal.empty()) {
                return false;
            }
        }
        else if (eq != std::string::npos) {
            auto key = trim(clause.substr(0, eq));
            auto literal = trim(clause.substr(eq + 1));
            if (!is_valid_condition_key(key) || literal.empty()) {
                return false;
            }
        }
        else {
            return false;
        }
    }
    return !clauses.empty();
}

static auto parse_stylesheet_syntax(std::string_view stylesheet) -> bool
{
    if (stylesheet.empty()) {
        return true;
    }

    const char* p = stylesheet.data();
    const char* end = p + stylesheet.size();

    auto skip_ws = [&] {
        while (p < end && std::isspace(static_cast<unsigned char>(*p))) {
            ++p;
        }
    };

    auto read_ident = [&] -> std::string {
        const char* s = p;
        while (p < end && (std::isalnum(static_cast<unsigned char>(*p)) || *p == '_' || *p == '-' || *p == '.' ||
                           *p == '#' || *p == '*')) {
            ++p;
        }
        return {s, static_cast<size_t>(p - s)};
    };

    while (p < end) {
        skip_ws();
        if (p >= end) {
            break;
        }

        auto sel = read_ident();
        if (sel.empty()) {
            return false;
        }

        skip_ws();
        if (p >= end || *p != '{') {
            return false;
        }
        ++p;

        bool has_decl = false;
        while (true) {
            skip_ws();
            if (p >= end) {
                return false;
            }
            if (*p == '}') {
                ++p;
                break;
            }

            const char* ks = p;
            while (p < end && *p != ':' && *p != '}' && !std::isspace(static_cast<unsigned char>(*p))) {
                ++p;
            }
            std::string key{ks, static_cast<size_t>(p - ks)};
            if (key.empty()) {
                return false;
            }

            skip_ws();
            if (p >= end || *p != ':') {
                return false;
            }
            ++p;
            skip_ws();

            const char* vs = p;
            while (p < end && *p != ';' && *p != '}') {
                ++p;
            }
            std::string val{vs, static_cast<size_t>(p - vs)};
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) {
                val.pop_back();
            }
            if (val.empty()) {
                return false;
            }

            if (p < end && *p == ';') {
                ++p;
            }
            has_decl = true;
        }
        if (!has_decl) {
            return false;
        }
    }
    return true;
}

static auto is_known_fidelity_mode(FidelityMode m) noexcept -> bool
{
    switch (m) {
    case FidelityMode::full:           return true;
    case FidelityMode::truncate:       return true;
    case FidelityMode::compact:        return true;
    case FidelityMode::summary_low:    return true;
    case FidelityMode::summary_medium: return true;
    case FidelityMode::summary_high:   return true;
    default:                           return false;
    }
}

auto validate(const Graph& graph, const ValidationConfig& config,
              std::vector<const LintRule*> extra_rules) -> std::vector<Diagnostic>
{
    std::vector<Diagnostic> diags;

    // -- ERROR rules -----------------------------------------------------------

    bool structural_errors = false;

    {
        std::vector<const Node*> starts;
        for (const auto& nv : graph.nodes) {
            const Node& n = to_base(nv);
            if (is_start_node(n)) {
                starts.push_back(&n);
            }
        }
        if (starts.size() != 1) {
            structural_errors = true;
            diags.push_back(
                {.rule_id = RuleId{"start_node"},
                 .severity = Severity::error,
                 .message =
                     DiagnosticMessage{starts.empty() ? "No start node found (shape=Mdiamond or id='start'/'Start')"
                                                      : "Multiple start nodes found; exactly one is required"},
                 .suggested_fix = SuggestedFix{"Add exactly one start node with shape=Mdiamond"}});
        }
    }

    {
        std::vector<const Node*> terminals;
        for (const auto& nv : graph.nodes) {
            const Node& n = to_base(nv);
            if (is_terminal_node(n)) {
                terminals.push_back(&n);
            }
        }
        if (terminals.size() != 1) {
            structural_errors = true;
            diags.push_back(
                {.rule_id = RuleId{"terminal_node"},
                 .severity = Severity::error,
                 .message =
                     DiagnosticMessage{terminals.empty() ? "No terminal node found (shape=Msquare or id='exit'/'end')"
                                                         : "Multiple terminal nodes found; exactly one is required"},
                 .suggested_fix = SuggestedFix{"Add exactly one terminal node with shape=Msquare"}});
        }
    }

    // A node matching both predicates is structurally invalid.
    for (const auto& nv : graph.nodes) {
        const Node& n = to_base(nv);
        if (is_start_node(n) && is_terminal_node(n)) {
            structural_errors = true;
            diags.push_back(
                {.rule_id = RuleId{"start_node"},
                 .severity = Severity::error,
                 .node_id = n.id,
                 .message =
                     DiagnosticMessage{"Node '" + type_safe::get(n.id) + "' matches both start and terminal criteria"},
                 .suggested_fix = SuggestedFix{"A node cannot be both a start and terminal node; use distinct nodes"}});
        }
    }

    std::unordered_set<std::string> node_ids;
    for (const auto& nv : graph.nodes) {
        node_ids.insert(type_safe::get(to_base(nv).id));
    }

    for (const auto& e : graph.edges) {
        if (!node_ids.count(type_safe::get(e.from))) {
            diags.push_back(
                {.rule_id = RuleId{"edge_target_exists"},
                 .severity = Severity::error,
                 .node_id = e.from,
                 .message = DiagnosticMessage{"Edge source '" + type_safe::get(e.from) + "' does not exist"},
                 .suggested_fix = SuggestedFix{"Ensure all edge targets reference existing node IDs"}});
        }
        if (!node_ids.count(type_safe::get(e.to))) {
            diags.push_back({.rule_id = RuleId{"edge_target_exists"},
                             .severity = Severity::error,
                             .node_id = e.from,
                             .to_node_id = e.to,
                             .message = DiagnosticMessage{"Edge target '" + type_safe::get(e.to) + "' does not exist"},
                             .suggested_fix = SuggestedFix{"Ensure all edge targets reference existing node IDs"}});
        }
    }

    std::unordered_set<std::string> has_incoming, has_outgoing;
    for (const auto& e : graph.edges) {
        has_incoming.insert(type_safe::get(e.to));
        has_outgoing.insert(type_safe::get(e.from));
    }

    for (const auto& nv : graph.nodes) {
        const Node& n = to_base(nv);
        const auto& id = type_safe::get(n.id);
        if (is_start_node(n) && has_incoming.count(id)) {
            diags.push_back({.rule_id = RuleId{"start_no_incoming"},
                             .severity = Severity::error,
                             .node_id = n.id,
                             .message = DiagnosticMessage{"Start node '" + id + "' must not have incoming edges"},
                             .suggested_fix = SuggestedFix{"Remove incoming edges from the start node"}});
        }
        if (is_terminal_node(n) && has_outgoing.count(id)) {
            diags.push_back({.rule_id = RuleId{"exit_no_outgoing"},
                             .severity = Severity::error,
                             .node_id = n.id,
                             .message = DiagnosticMessage{"Terminal node '" + id + "' must not have outgoing edges"},
                             .suggested_fix = SuggestedFix{"Remove outgoing edges from the terminal node"}});
        }
    }

    for (const auto& e : graph.edges) {
        const auto& cond = type_safe::get(e.condition);
        if (!cond.empty() && !parse_condition_syntax(cond)) {
            const auto& from_str = type_safe::get(e.from);
            const auto& to_str = type_safe::get(e.to);
            diags.push_back(
                {.rule_id = RuleId{"condition_syntax"},
                 .severity = Severity::error,
                 .node_id = from_str.empty() ? std::optional<NodeId>{} : std::optional<NodeId>{e.from},
                 .to_node_id = to_str.empty() ? std::optional<NodeId>{} : std::optional<NodeId>{e.to},
                 .message = DiagnosticMessage{"Edge condition '" + cond + "' is not valid syntax"},
                 .suggested_fix = SuggestedFix{
                     "Use Key Op Literal format; Key must be outcome, preferred_label, or context.<path>"}});
        }
    }

    {
        const auto& ss = type_safe::get(graph.model_stylesheet);
        if (!ss.empty() && !parse_stylesheet_syntax(ss)) {
            diags.push_back({.rule_id = RuleId{"stylesheet_syntax"},
                             .severity = Severity::error,
                             .message = DiagnosticMessage{"model_stylesheet is not valid CSS-like syntax"},
                             .suggested_fix = SuggestedFix{"Use 'selector { key: value; }' format"}});
        }
    }

    // -- WARNING rules ---------------------------------------------------------
    // Skip when start/terminal structural errors are present -- BFS and other
    // rules require a well-formed graph topology to produce meaningful results.

    if (structural_errors) {
        for (const auto* rule : extra_rules) {
            if (rule == nullptr) {
                continue;
            }
            auto extra = rule->apply(graph);
            diags.insert(diags.end(), std::make_move_iterator(extra.begin()), std::make_move_iterator(extra.end()));
        }
        return diags;
    }

    {
        const Node* start = nullptr;
        for (const auto& nv : graph.nodes) {
            const Node& n = to_base(nv);
            if (is_start_node(n)) {
                start = &n;
                break;
            }
        }
        if (start) {
            auto reachable = reachable_ids(graph, start->id);
            for (const auto& nv : graph.nodes) {
                const Node& n = to_base(nv);
                const auto& nid = type_safe::get(n.id);
                if (!is_start_node(n) && !reachable.count(nid)) {
                    diags.push_back({.rule_id = RuleId{"reachability"},
                                     .severity = Severity::warning,
                                     .node_id = n.id,
                                     .message = DiagnosticMessage{"Node '" + nid + "' is unreachable from start"},
                                     .suggested_fix = SuggestedFix{"Remove or connect the unreachable node"}});
                }
            }
        }
    }

    if (!config.known_types.empty()) {
        std::unordered_set<std::string> known;
        for (const auto& t : config.known_types) {
            known.insert(type_safe::get(t));
        }
        for (const auto& nv : graph.nodes) {
            const Node& n = to_base(nv);
            const auto& nt = type_safe::get(n.node_type);
            if (!nt.empty() && !known.count(nt)) {
                diags.push_back(
                    {.rule_id = RuleId{"type_known"},
                     .severity = Severity::warning,
                     .node_id = n.id,
                     .message = DiagnosticMessage{"Node type '" + nt + "' is not registered in the handler registry"},
                     .suggested_fix = SuggestedFix{"Register this handler type or correct the type attribute"}});
            }
        }
    }

    for (const auto& nv : graph.nodes) {
        const Node& n = to_base(nv);
        const auto& nt = type_safe::get(n.node_type);
        if (!nt.empty()) {
            if (auto expected = shape_for_handler_type(nt); expected && *expected != n.shape) {
                diags.push_back(
                    {.rule_id = RuleId{"shape_type_consistency"},
                     .severity = Severity::error,
                     .node_id = n.id,
                     .message = DiagnosticMessage{"Node '" + type_safe::get(n.id) +
                                                  "' has type='" + nt +
                                                  "' but shape does not match (handler would perform an unsafe cast)"},
                     .suggested_fix = SuggestedFix{"Remove the explicit shape= attribute or align it with the type= value"}});
            }
        }
    }

    for (const auto& nv : graph.nodes) {
        const Node& n = to_base(nv);
        if (n.fidelity.has_value() && !is_known_fidelity_mode(*n.fidelity)) {
            diags.push_back({.rule_id = RuleId{"fidelity_valid"},
                             .severity = Severity::warning,
                             .node_id = n.id,
                             .message = DiagnosticMessage{"fidelity attribute has unrecognised value"},
                             .suggested_fix = SuggestedFix{"Use one of: full, truncate, compact, summary:low, summary:medium, summary:high"}});
        }
    }
    for (const auto& e : graph.edges) {
        if (e.fidelity.has_value() && !is_known_fidelity_mode(*e.fidelity)) {
            diags.push_back({.rule_id = RuleId{"fidelity_valid"},
                             .severity = Severity::warning,
                             .node_id = e.from,
                             .message = DiagnosticMessage{"edge fidelity attribute has unrecognised value"},
                             .suggested_fix = SuggestedFix{"Use one of: full, truncate, compact, summary:low, summary:medium, summary:high"}});
        }
    }
    if (graph.default_fidelity.has_value() && !is_known_fidelity_mode(*graph.default_fidelity)) {
        diags.push_back({.rule_id = RuleId{"fidelity_valid"},
                         .severity = Severity::warning,
                         .message = DiagnosticMessage{"graph default_fidelity attribute has unrecognised value"},
                         .suggested_fix = SuggestedFix{"Use one of: full, truncate, compact, summary:low, summary:medium, summary:high"}});
    }

    auto check_retry_target = [&](const NodeId& target, const NodeId& subject, const char* attr) {
        if (!type_safe::get(target).empty() && !node_ids.count(type_safe::get(target))) {
            diags.push_back(
                {.rule_id = RuleId{"retry_target_exists"},
                 .severity = Severity::warning,
                 .node_id = subject,
                 .message = DiagnosticMessage{std::string{attr} + " '" + type_safe::get(target) + "' does not exist"},
                 .suggested_fix = SuggestedFix{"Ensure retry_target references an existing node ID"}});
        }
    };
    for (const auto& nv : graph.nodes) {
        const Node& n = to_base(nv);
        check_retry_target(n.retry_target, n.id, "retry_target");
        check_retry_target(n.fallback_retry_target, n.id, "fallback_retry_target");
    }
    if (!type_safe::get(graph.retry_target).empty() && !node_ids.count(type_safe::get(graph.retry_target))) {
        diags.push_back(
            {.rule_id = RuleId{"retry_target_exists"},
             .severity = Severity::warning,
             .message =
                 DiagnosticMessage{"graph.retry_target '" + type_safe::get(graph.retry_target) + "' does not exist"},
             .suggested_fix = SuggestedFix{"Ensure graph-level retry_target references an existing node ID"}});
    }
    if (!type_safe::get(graph.fallback_retry_target).empty() &&
        !node_ids.count(type_safe::get(graph.fallback_retry_target))) {
        diags.push_back(
            {.rule_id = RuleId{"retry_target_exists"},
             .severity = Severity::warning,
             .message = DiagnosticMessage{"graph.fallback_retry_target '" +
                                          type_safe::get(graph.fallback_retry_target) + "' does not exist"},
             .suggested_fix = SuggestedFix{"Ensure graph-level fallback_retry_target references an existing node ID"}});
    }

    for (const auto& nv : graph.nodes) {
        const Node& n = to_base(nv);
        if (n.goal_gate) {
            const bool node_has_retry =
                !type_safe::get(n.retry_target).empty() || !type_safe::get(n.fallback_retry_target).empty();
            const bool graph_has_retry =
                !type_safe::get(graph.retry_target).empty() || !type_safe::get(graph.fallback_retry_target).empty();
            if (!node_has_retry && !graph_has_retry) {
                diags.push_back(
                    {.rule_id = RuleId{"goal_gate_has_retry"},
                     .severity = Severity::warning,
                     .node_id = n.id,
                     .message = DiagnosticMessage{"goal_gate node '" + type_safe::get(n.id) + "' has no retry_target"},
                     .suggested_fix = SuggestedFix{"Set retry_target on the node or set graph-level retry_target"}});
            }
        }
    }

    for (const auto& nv : graph.nodes) {
        const Node& n = to_base(nv);
        if (is_start_node(n) || is_terminal_node(n)) {
            continue;
        }
        const auto& lbl = type_safe::get(n.label);
        const bool label_is_auto = lbl.empty() || lbl == type_safe::get(n.id);
        if (const auto* cn = std::get_if<CodergenNode>(&nv)) {
            if (type_safe::get(cn->prompt).empty() && label_is_auto) {
                diags.push_back({.rule_id = RuleId{"prompt_on_llm_nodes"},
                                 .severity = Severity::warning,
                                 .node_id = n.id,
                                 .message = DiagnosticMessage{"Codergen node '" + type_safe::get(n.id) +
                                                              "' has neither prompt nor label"},
                                 .suggested_fix = SuggestedFix{"Add a prompt or label attribute to this codergen node"}});
            }
        }
    }

    // -- Extra / custom rules --------------------------------------------------

    for (const auto* rule : extra_rules) {
        if (rule == nullptr) {
            continue;
        }
        auto extra = rule->apply(graph);
        diags.insert(diags.end(), std::make_move_iterator(extra.begin()), std::make_move_iterator(extra.end()));
    }

    return diags;
}

void validate_or_raise(const Graph& graph, const ValidationConfig& config, std::vector<const LintRule*> extra_rules)
{
    auto diags = validate(graph, config, extra_rules);
    std::vector<Diagnostic> errors;
    for (auto& d : diags) {
        if (d.severity == Severity::error) {
            errors.push_back(std::move(d));
        }
    }
    if (!errors.empty()) {
        throw ValidationError{std::move(errors)};
    }
}

}  // namespace attractor
