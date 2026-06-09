#include <attractor/transform.hpp>

#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace attractor {
namespace detail {

static auto replace_all(std::string s, std::string_view from, std::string_view to) -> std::string
{
    if (from.empty()) {
        return s;
    }
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
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

static auto parse_stylesheet(std::string_view ss) -> std::vector<StyleRule>
{
    std::vector<StyleRule> rules;
    const char* p = ss.data();
    const char* end = p + ss.size();

    auto skip_ws = [&] {
        while (p < end && std::isspace(static_cast<unsigned char>(*p))) {
            ++p;
        }
    };

    auto read_selector = [&] -> std::string {
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

        auto sel = read_selector();
        if (sel.empty()) {
            break;
        }

        StyleRule rule;
        if (sel == "*") {
            rule.type = StyleRule::SelectorType::universal;
            rule.specificity = 0;
            rule.selector_value.clear();
        }
        else if (!sel.empty() && sel[0] == '#') {
            rule.type = StyleRule::SelectorType::id;
            rule.specificity = 3;
            rule.selector_value = sel.substr(1);
        }
        else if (!sel.empty() && sel[0] == '.') {
            rule.type = StyleRule::SelectorType::css_class;
            rule.specificity = 2;
            rule.selector_value = sel.substr(1);
        }
        else {
            rule.type = StyleRule::SelectorType::shape;
            rule.specificity = 1;
            rule.selector_value = sel;
        }

        skip_ws();
        if (p >= end || *p != '{') {
            break;
        }
        ++p;

        while (true) {
            skip_ws();
            if (p >= end) {
                break;
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
                break;
            }

            skip_ws();
            if (p >= end || *p != ':') {
                break;
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

            if (p < end && *p == ';') {
                ++p;
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

        rules.push_back(std::move(rule));
    }

    return rules;
}

static auto node_matches_rule(const Node& node, const StyleRule& rule) -> bool
{
    switch (rule.type) {
    case StyleRule::SelectorType::universal:
        return true;
    case StyleRule::SelectorType::shape:
        return type_safe::get(node.shape) == rule.selector_value;
    case StyleRule::SelectorType::id:
        return type_safe::get(node.id) == rule.selector_value;
    case StyleRule::SelectorType::css_class: {
        const auto& cls = type_safe::get(node.css_class);
        if (cls.empty()) {
            return false;
        }
        size_t start = 0;
        while (start <= cls.size()) {
            auto comma = cls.find(',', start);
            auto token = cls.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front()))) {
                token.erase(0, 1);
            }
            while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back()))) {
                token.pop_back();
            }
            if (token == rule.selector_value) {
                return true;
            }
            if (comma == std::string::npos) {
                break;
            }
            start = comma + 1;
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
    for (auto& node : out.nodes) {
        auto expanded = detail::replace_all(type_safe::get(node.prompt), "$goal", goal);
        node.prompt = PromptText{std::move(expanded)};
    }
    return out;
}

auto StylesheetTransform::apply(const Graph& graph) const -> Graph
{
    const auto& ss = type_safe::get(graph.model_stylesheet);
    if (ss.empty()) {
        return graph;
    }

    auto rules = detail::parse_stylesheet(ss);
    Graph out = graph;

    for (auto& node : out.nodes) {
        // For each property, find the winning rule: highest specificity wins;
        // among equal specificity, the later rule wins (stable: iterate in order, last survives).
        std::string best_model, best_provider, best_effort;
        int spec_model = -1, spec_provider = -1, spec_effort = -1;

        for (const auto& rule : rules) {
            if (!detail::node_matches_rule(node, rule)) {
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

        if (!best_model.empty() && type_safe::get(node.llm_model).empty()) {
            node.llm_model = LlmModel{best_model};
        }
        if (!best_provider.empty() && type_safe::get(node.llm_provider).empty()) {
            node.llm_provider = LlmProvider{best_provider};
        }
        if (!best_effort.empty() && !node.reasoning_effort.has_value()) {
            if (best_effort == "low") {
                node.reasoning_effort = ReasoningEffort::low;
            }
            else if (best_effort == "medium") {
                node.reasoning_effort = ReasoningEffort::medium;
            }
            else if (best_effort == "high") {
                node.reasoning_effort = ReasoningEffort::high;
            }
        }
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
