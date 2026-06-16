#include "attractor_test_support.hpp"
#include <algorithm>
#include <attractor/dot_parser.hpp>
#include <attractor/graph.hpp>
#include <attractor/transform.hpp>
#include <string>

using namespace attractor;

// -- helpers --------------------------------------------------------------------

static Graph make_graph_with_goal(std::string_view goal_text, std::string_view prompt_text)
{
    auto result = parse_graph(std::string{"digraph g { graph [goal=\""} + std::string{goal_text} +
                              "\"] start [shape=Mdiamond] work [shape=box, prompt=\"" + std::string{prompt_text} +
                              "\"] done [shape=Msquare] start -> work -> done }");
    SNITCH_REQUIRE(result.has_value());
    return std::move(*result);
}

static Graph make_valid_linear()
{
    auto result = parse_graph(R"(
        digraph g {
            start [shape=Mdiamond]
            work  [shape=box, prompt="Do work"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    return std::move(*result);
}

// -- AC1: variable expansion ----------------------------------------------------

SNITCH_TEST_CASE("[transform] variable expansion replaces $goal in prompt")
{
    auto g = make_graph_with_goal("Write tests", "Plan how to: $goal");
    VariableExpansionTransform xform;
    auto out = xform.apply(g);
    auto it =
        std::find_if(out.nodes.begin(), out.nodes.end(), [](const Node& n) { return type_safe::get(n.id) == "work"; });
    SNITCH_REQUIRE(it != out.nodes.end());
    SNITCH_CHECK(type_safe::get(it->prompt) == "Plan how to: Write tests");
}

SNITCH_TEST_CASE("[transform] variable expansion -- no $goal in prompt -- unchanged")
{
    auto g = make_graph_with_goal("Write tests", "Do something else");
    VariableExpansionTransform xform;
    auto out = xform.apply(g);
    auto it =
        std::find_if(out.nodes.begin(), out.nodes.end(), [](const Node& n) { return type_safe::get(n.id) == "work"; });
    SNITCH_REQUIRE(it != out.nodes.end());
    SNITCH_CHECK(type_safe::get(it->prompt) == "Do something else");
}

SNITCH_TEST_CASE("[transform] variable expansion -- multiple $goal occurrences -- all replaced")
{
    auto g = make_graph_with_goal("run tests", "First $goal, then $goal again");
    VariableExpansionTransform xform;
    auto out = xform.apply(g);
    auto it =
        std::find_if(out.nodes.begin(), out.nodes.end(), [](const Node& n) { return type_safe::get(n.id) == "work"; });
    SNITCH_REQUIRE(it != out.nodes.end());
    SNITCH_CHECK(type_safe::get(it->prompt) == "First run tests, then run tests again");
}

SNITCH_TEST_CASE("[transform] variable expansion -- empty goal -- $goal replaced with empty string")
{
    auto g = make_graph_with_goal("", "Do $goal now");
    VariableExpansionTransform xform;
    auto out = xform.apply(g);
    auto it =
        std::find_if(out.nodes.begin(), out.nodes.end(), [](const Node& n) { return type_safe::get(n.id) == "work"; });
    SNITCH_REQUIRE(it != out.nodes.end());
    SNITCH_CHECK(type_safe::get(it->prompt) == "Do  now");
}

// -- AC3: immutability ---------------------------------------------------------

SNITCH_TEST_CASE("[transform] apply() does not mutate input graph")
{
    auto g = make_graph_with_goal("Write tests", "Plan: $goal");
    const std::string original_prompt = type_safe::get(std::find_if(g.nodes.begin(), g.nodes.end(), [](const Node& n) {
                                                           return type_safe::get(n.id) == "work";
                                                       })->prompt);

    VariableExpansionTransform xform;
    [[maybe_unused]] auto out = xform.apply(g);

    auto it =
        std::find_if(g.nodes.begin(), g.nodes.end(), [](const Node& n) { return type_safe::get(n.id) == "work"; });
    SNITCH_REQUIRE(it != g.nodes.end());
    SNITCH_CHECK(type_safe::get(it->prompt) == original_prompt);
}

// -- AC2: custom transforms ----------------------------------------------------

SNITCH_TEST_CASE("[transform] custom transform runs after built-ins")
{
    struct AppendCustom final : public Transform {
        [[nodiscard]] auto apply(const Graph& g) const -> Graph override
        {
            Graph out = g;
            for (auto& n : out.nodes) {
                if (!type_safe::get(n.prompt).empty()) {
                    n.prompt = PromptText{type_safe::get(n.prompt) + " CUSTOM"};
                }
            }
            return out;
        }
    };

    AppendCustom custom;
    auto g = make_graph_with_goal("tests", "Plan: $goal");
    auto out = apply_transforms(g, {&custom});
    auto it =
        std::find_if(out.nodes.begin(), out.nodes.end(), [](const Node& n) { return type_safe::get(n.id) == "work"; });
    SNITCH_REQUIRE(it != out.nodes.end());
    SNITCH_CHECK(type_safe::get(it->prompt) == "Plan: tests CUSTOM");
}

SNITCH_TEST_CASE("[transform] two custom transforms run in registration order")
{
    struct Append1 final : public Transform {
        [[nodiscard]] auto apply(const Graph& g) const -> Graph override
        {
            Graph out = g;
            for (auto& n : out.nodes) {
                if (!type_safe::get(n.prompt).empty()) {
                    n.prompt = PromptText{type_safe::get(n.prompt) + "_1"};
                }
            }
            return out;
        }
    };

    struct Append2 final : public Transform {
        [[nodiscard]] auto apply(const Graph& g) const -> Graph override
        {
            Graph out = g;
            for (auto& n : out.nodes) {
                if (!type_safe::get(n.prompt).empty()) {
                    n.prompt = PromptText{type_safe::get(n.prompt) + "_2"};
                }
            }
            return out;
        }
    };

    Append1 t1;
    Append2 t2;
    auto g = make_valid_linear();
    auto out = apply_transforms(g, {&t1, &t2});
    auto it =
        std::find_if(out.nodes.begin(), out.nodes.end(), [](const Node& n) { return type_safe::get(n.id) == "work"; });
    SNITCH_REQUIRE(it != out.nodes.end());
    SNITCH_CHECK(type_safe::get(it->prompt) == "Do work_1_2");
}

// -- AC4: stylesheet transform -------------------------------------------------

SNITCH_TEST_CASE("[transform] stylesheet universal rule sets llm_model on all nodes")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { llm_model: claude-sonnet-4-6; }"]
            start [shape=Mdiamond]
            work  [shape=box, prompt="Do work"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    for (const auto& n : out.nodes) {
        SNITCH_CHECK(type_safe::get(n.llm_model) == "claude-sonnet-4-6");
    }
}

SNITCH_TEST_CASE("[transform] stylesheet shape rule overrides universal on matching nodes")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { llm_model: base-model; } box { llm_model: box-model; }"]
            start [shape=Mdiamond]
            work  [shape=box, prompt="Do work"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    for (const auto& n : out.nodes) {
        if (n.shape == NodeShape::box) {
            SNITCH_CHECK(type_safe::get(n.llm_model) == "box-model");
        }
        else {
            SNITCH_CHECK(type_safe::get(n.llm_model) == "base-model");
        }
    }
}

SNITCH_TEST_CASE("[transform] explicit node llm_model not overwritten by stylesheet")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { llm_model: stylesheet-model; }"]
            start [shape=Mdiamond]
            work  [shape=box, prompt="Do work", llm_model="explicit-model"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    auto it =
        std::find_if(out.nodes.begin(), out.nodes.end(), [](const Node& n) { return type_safe::get(n.id) == "work"; });
    SNITCH_REQUIRE(it != out.nodes.end());
    SNITCH_CHECK(type_safe::get(it->llm_model) == "explicit-model");
}

SNITCH_TEST_CASE("[transform] apply_transforms with no stylesheet -- llm fields unchanged")
{
    auto g = make_valid_linear();
    auto out = apply_transforms(g);
    for (const auto& n : out.nodes) {
        SNITCH_CHECK(type_safe::get(n.llm_model).empty());
    }
}

SNITCH_TEST_CASE("[transform] apply_transforms with no custom transforms runs both built-ins")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [goal="run tests", model_stylesheet="* { llm_model: m1; }"]
            start [shape=Mdiamond]
            work  [shape=box, prompt="Plan: $goal"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    auto out = apply_transforms(*result);
    auto it =
        std::find_if(out.nodes.begin(), out.nodes.end(), [](const Node& n) { return type_safe::get(n.id) == "work"; });
    SNITCH_REQUIRE(it != out.nodes.end());
    SNITCH_CHECK(type_safe::get(it->prompt) == "Plan: run tests");
    SNITCH_CHECK(type_safe::get(it->llm_model) == "m1");
}

SNITCH_TEST_CASE("[transform] stylesheet reasoning_effort property applied to nodes")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { reasoning_effort: medium; }"]
            start [shape=Mdiamond]
            work  [shape=box, prompt="Do work"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    for (const auto& n : out.nodes) {
        SNITCH_REQUIRE(n.reasoning_effort.has_value());
        SNITCH_CHECK(*n.reasoning_effort == ReasoningEffort::medium);
    }
}

SNITCH_TEST_CASE("[transform] stylesheet equal-specificity: later rule overwrites earlier rule")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { llm_model: first; } * { llm_model: second; }"]
            start [shape=Mdiamond]
            work  [shape=box, prompt="Do work"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    for (const auto& n : out.nodes) {
        SNITCH_CHECK(type_safe::get(n.llm_model) == "second");
    }
}

SNITCH_TEST_CASE("[transform] stylesheet class selector wins over universal -- 4.3-U-001")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { reasoning_effort: medium; } .fast { reasoning_effort: low; }"]
            start [shape=Mdiamond]
            work  [shape=box, class="fast", prompt="Do work"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    auto it = std::find_if(out.nodes.begin(), out.nodes.end(),
        [](const Node& n) { return type_safe::get(n.id) == "work"; });
    SNITCH_REQUIRE(it != out.nodes.end());
    SNITCH_REQUIRE(it->reasoning_effort.has_value());
    SNITCH_CHECK(*it->reasoning_effort == ReasoningEffort::low);
    for (const auto& n : out.nodes) {
        if (type_safe::get(n.id) == "work") { continue; }
        SNITCH_CHECK(n.reasoning_effort.has_value());
        SNITCH_CHECK(*n.reasoning_effort == ReasoningEffort::medium);
    }
}

SNITCH_TEST_CASE("[transform] stylesheet ID selector wins over class shape and universal -- 4.3-U-002")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { llm_model: universal; } box { llm_model: shape; } .fast { llm_model: class; } #review { llm_model: id-model; }"]
            start  [shape=Mdiamond]
            review [shape=box, class="fast"]
            done   [shape=Msquare]
            start -> review -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    auto rev = std::find_if(out.nodes.begin(), out.nodes.end(),
        [](const Node& n) { return type_safe::get(n.id) == "review"; });
    SNITCH_REQUIRE(rev != out.nodes.end());
    SNITCH_CHECK(type_safe::get(rev->llm_model) == "id-model");
    for (const auto& n : out.nodes) {
        if (type_safe::get(n.id) == "review") { continue; }
        SNITCH_CHECK(type_safe::get(n.llm_model) == "universal");
    }
}
