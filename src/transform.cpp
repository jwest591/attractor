#include <attractor/transform.hpp>

#include <algorithm>
#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace attractor {
namespace detail {

static auto replace_all(std::string_view s, std::string_view from, std::string_view to) -> std::string
{
    if (from.empty()) {
        return std::string{s};
    }
    std::string result;
    result.reserve(s.size());
    const auto* it = s.cbegin();
    while (true) {
        auto found = std::ranges::search(it, s.cend(), from.cbegin(), from.cend());
        result.append(it, found.begin());
        if (found.empty()) {
            break;
        }
        result.append(to);
        it = found.end();
    }
    return result;
}

struct StyleRule {
    enum class SelectorType { universal, shape, css_class, id };
    SelectorType type = SelectorType::universal;
    std::string selector_value;
    int specificity = 0;

    std::string llm_model;
    std::string llm_provider;
    std::string reasoning_effort;
};

static constexpr std::string_view k_whitespace = " \t\n\r";
static constexpr std::string_view k_selector_chars =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.*#";

static auto skip_ws(std::string_view& sv) -> void
{
    auto pos = sv.find_first_not_of(k_whitespace);
    sv.remove_prefix(pos == std::string_view::npos ? sv.size() : pos);
}

static auto consume_while(std::string_view& sv, std::string_view chars) -> std::string_view
{
    auto end = sv.find_first_not_of(chars);
    auto token = sv.substr(0, end == std::string_view::npos ? sv.size() : end);
    sv.remove_prefix(token.size());
    return token;
}

static auto consume_until(std::string_view& sv, std::string_view stops) -> std::string_view
{
    auto end = sv.find_first_of(stops);
    auto token = sv.substr(0, end == std::string_view::npos ? sv.size() : end);
    sv.remove_prefix(token.size());
    return token;
}

static auto rtrim(std::string_view sv) -> std::string_view
{
    auto end = sv.find_last_not_of(k_whitespace);
    return end == std::string_view::npos ? sv.substr(0, 0) : sv.substr(0, end + 1);
}

static auto parse_stylesheet(const StylesheetId& ssid) -> std::vector<StyleRule>
{
    std::vector<StyleRule> rules;

    std::string_view ss = type_safe::get(ssid);
    while (true) {
        skip_ws(ss);
        if (ss.empty()) {
            break;
        }

        auto sel = consume_while(ss, k_selector_chars);
        if (sel.empty()) {
            break;
        }

        StyleRule rule;
        if (sel == "*") {
            rule.type = StyleRule::SelectorType::universal;
            rule.specificity = 0;
        }
        else if (sel.starts_with('#')) {
            rule.type = StyleRule::SelectorType::id;
            rule.specificity = 3;
            rule.selector_value = sel.substr(1);
        }
        else if (sel.starts_with('.')) {
            rule.type = StyleRule::SelectorType::css_class;
            rule.specificity = 2;
            rule.selector_value = sel.substr(1);
        }
        else {
            rule.type = StyleRule::SelectorType::shape;
            rule.specificity = 1;
            rule.selector_value = sel;
        }

        skip_ws(ss);
        if (ss.empty() || ss.front() != '{') {
            break;
        }
        ss.remove_prefix(1);

        while (!ss.empty() && ss.front() != '}') {
            skip_ws(ss);
            if (ss.empty() || ss.front() == '}') {
                break;
            }

            auto key = consume_until(ss, ": \t\n\r}");
            if (key.empty()) {
                break;
            }

            skip_ws(ss);
            if (ss.empty() || ss.front() != ':') {
                break;
            }
            ss.remove_prefix(1);
            skip_ws(ss);

            auto val = std::string{rtrim(consume_until(ss, ";}\n"))};

            if (!ss.empty() && ss.front() == ';') {
                ss.remove_prefix(1);
            }

            if (key == "llm_model") {
                rule.llm_model = std::move(val);
            }
            else if (key == "llm_provider") {
                rule.llm_provider = std::move(val);
            }
            else if (key == "reasoning_effort") {
                rule.reasoning_effort = std::move(val);
            }
        }

        if (!ss.empty() && ss.front() == '}') {
            ss.remove_prefix(1);
        }
        rules.push_back(std::move(rule));
    }

    return rules;
}

static auto node_matches_rule(const NodeVariant& nv, const StyleRule& rule) -> bool
{
    const Node& node = to_base(nv);
    switch (rule.type) {
    case StyleRule::SelectorType::universal:
        return true;
    case StyleRule::SelectorType::shape:
        return node_shape_to_string(node.shape) == rule.selector_value;
    case StyleRule::SelectorType::id:
        return type_safe::get(node.id) == rule.selector_value;
    case StyleRule::SelectorType::css_class: {
        std::string_view remaining{type_safe::get(node.css_class)};
        while (!remaining.empty()) {
            auto comma = remaining.find(',');
            auto token = remaining.substr(0, comma == std::string_view::npos ? remaining.size() : comma);
            skip_ws(token);
            if (rtrim(token) == rule.selector_value) {
                return true;
            }
            if (comma == std::string_view::npos) {
                break;
            }
            remaining.remove_prefix(comma + 1);
        }
        return false;
    }
    }
    return false;
}

}  // namespace detail

auto VariableExpansionTransform::apply(const Graph& graph) const -> Graph
{
    Graph out = graph;
    const auto& goal = type_safe::get(graph.goal);
    for (auto& nv : out.nodes) {
        std::visit([&goal](auto& derived) {
            using T = std::decay_t<decltype(derived)>;
            if constexpr (std::is_same_v<T, CodergenNode> || std::is_same_v<T, FanInNode>) {
                auto expanded = detail::replace_all(type_safe::get(derived.prompt), "$goal", goal);
                derived.prompt = PromptText{std::move(expanded)};
            }
        }, nv);
    }
    return out;
}

auto StylesheetTransform::apply(const Graph& graph) const -> Graph
{
    if (graph.model_stylesheet.empty()) {
        return graph;
    }

    auto rules = detail::parse_stylesheet(graph.model_stylesheet);
    Graph out = graph;

    for (auto& nv : out.nodes) {
        // For each property, find the winning rule: highest specificity wins;
        // among equal specificity, the later rule wins (stable: iterate in order, last survives).
        std::string best_model, best_provider, best_effort;
        int spec_model = -1, spec_provider = -1, spec_effort = -1;

        for (const auto& rule : rules) {
            if (!detail::node_matches_rule(nv, rule)) {
                continue;
            }
            if (!rule.llm_model.empty() && rule.specificity >= spec_model) {
                best_model = rule.llm_model;
                spec_model = rule.specificity;
            }
            if (!rule.llm_provider.empty() && rule.specificity >= spec_provider) {
                best_provider = rule.llm_provider;
                spec_provider = rule.specificity;
            }
            if (!rule.reasoning_effort.empty() && rule.specificity >= spec_effort) {
                best_effort = rule.reasoning_effort;
                spec_effort = rule.specificity;
            }
        }

        // LLM fields only apply to CodergenNode
        std::visit([&](auto& derived) {
            using T = std::decay_t<decltype(derived)>;
            if constexpr (std::is_same_v<T, CodergenNode>) {
                if (!best_model.empty() && derived.llm_model.empty()) {
                    derived.llm_model = LlmModel{best_model};
                }
                if (!best_provider.empty() && derived.llm_provider.empty()) {
                    derived.llm_provider = LlmProvider{best_provider};
                }
                if (!best_effort.empty() && !derived.reasoning_effort.has_value()) {
                    if (best_effort == "low") {
                        derived.reasoning_effort = ReasoningEffort::low;
                    }
                    else if (best_effort == "medium") {
                        derived.reasoning_effort = ReasoningEffort::medium;
                    }
                    else if (best_effort == "high") {
                        derived.reasoning_effort = ReasoningEffort::high;
                    }
                }
            }
        }, nv);
    }

    return out;
}

auto apply_transforms(const Graph& graph, const std::vector<const Transform*>& custom_transforms) -> Graph
{
    auto g = VariableExpansionTransform{}.apply(graph);
    g = StylesheetTransform{}.apply(g);
    for (const auto* t : custom_transforms) {
        if (t) {
            g = t->apply(g);
        }
    }
    return g;
}

}  // namespace attractor
