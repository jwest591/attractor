#include "attractor_test_support.hpp"
#include <algorithm>
#include <attractor/dot_parser.hpp>
#include <attractor/graph.hpp>
#include <attractor/transform.hpp>
#include <string>
#include <type_traits>
#include <variant>

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

static const CodergenNode* find_codergen_node(const Graph& g, std::string_view id)
{
    for (const auto& nv : g.nodes) {
        if (type_safe::get(to_base(nv).id) == id) {
            return std::get_if<CodergenNode>(&nv);
        }
    }
    return nullptr;
}

// -- AC1: variable expansion ----------------------------------------------------

SNITCH_TEST_CASE("[transform] variable expansion replaces $goal in prompt")
{
    auto g = make_graph_with_goal("Write tests", "Plan how to: $goal");
    VariableExpansionTransform xform;
    auto out = xform.apply(g);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->prompt) == "Plan how to: Write tests");
}

SNITCH_TEST_CASE("[transform] variable expansion -- no $goal in prompt -- unchanged")
{
    auto g = make_graph_with_goal("Write tests", "Do something else");
    VariableExpansionTransform xform;
    auto out = xform.apply(g);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->prompt) == "Do something else");
}

SNITCH_TEST_CASE("[transform] variable expansion -- multiple $goal occurrences -- all replaced")
{
    auto g = make_graph_with_goal("run tests", "First $goal, then $goal again");
    VariableExpansionTransform xform;
    auto out = xform.apply(g);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->prompt) == "First run tests, then run tests again");
}

SNITCH_TEST_CASE("[transform] variable expansion -- empty goal -- $goal replaced with empty string")
{
    auto g = make_graph_with_goal("", "Do $goal now");
    VariableExpansionTransform xform;
    auto out = xform.apply(g);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->prompt) == "Do  now");
}

// -- AC3: immutability ---------------------------------------------------------

SNITCH_TEST_CASE("[transform] apply() does not mutate input graph")
{
    auto g = make_graph_with_goal("Write tests", "Plan: $goal");
    const auto* orig_cn = find_codergen_node(g, "work");
    SNITCH_REQUIRE(orig_cn != nullptr);
    const std::string original_prompt = type_safe::get(orig_cn->prompt);

    VariableExpansionTransform xform;
    [[maybe_unused]] auto out = xform.apply(g);

    const auto* cn = find_codergen_node(g, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->prompt) == original_prompt);
}

// -- AC2: custom transforms ----------------------------------------------------

SNITCH_TEST_CASE("[transform] custom transform runs after built-ins")
{
    struct AppendCustom final : public Transform {
        [[nodiscard]] auto apply(const Graph& g) const -> Graph override
        {
            Graph out = g;
            for (auto& nv : out.nodes) {
                std::visit([](auto& derived) {
                    using T = std::decay_t<decltype(derived)>;
                    if constexpr (std::is_same_v<T, CodergenNode> || std::is_same_v<T, FanInNode>) {
                        if (!type_safe::get(derived.prompt).empty()) {
                            derived.prompt = PromptText{type_safe::get(derived.prompt) + " CUSTOM"};
                        }
                    }
                }, nv);
            }
            return out;
        }
    };

    AppendCustom custom;
    auto g = make_graph_with_goal("tests", "Plan: $goal");
    auto out = apply_transforms(g, {&custom});
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->prompt) == "Plan: tests CUSTOM");
}

SNITCH_TEST_CASE("[transform] two custom transforms run in registration order")
{
    struct Append1 final : public Transform {
        [[nodiscard]] auto apply(const Graph& g) const -> Graph override
        {
            Graph out = g;
            for (auto& nv : out.nodes) {
                std::visit([](auto& derived) {
                    using T = std::decay_t<decltype(derived)>;
                    if constexpr (std::is_same_v<T, CodergenNode> || std::is_same_v<T, FanInNode>) {
                        if (!type_safe::get(derived.prompt).empty()) {
                            derived.prompt = PromptText{type_safe::get(derived.prompt) + "_1"};
                        }
                    }
                }, nv);
            }
            return out;
        }
    };

    struct Append2 final : public Transform {
        [[nodiscard]] auto apply(const Graph& g) const -> Graph override
        {
            Graph out = g;
            for (auto& nv : out.nodes) {
                std::visit([](auto& derived) {
                    using T = std::decay_t<decltype(derived)>;
                    if constexpr (std::is_same_v<T, CodergenNode> || std::is_same_v<T, FanInNode>) {
                        if (!type_safe::get(derived.prompt).empty()) {
                            derived.prompt = PromptText{type_safe::get(derived.prompt) + "_2"};
                        }
                    }
                }, nv);
            }
            return out;
        }
    };

    Append1 t1;
    Append2 t2;
    auto g = make_valid_linear();
    auto out = apply_transforms(g, {&t1, &t2});
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->prompt) == "Do work_1_2");
}

// -- AC4: stylesheet transform -------------------------------------------------

SNITCH_TEST_CASE("[transform] stylesheet universal rule sets llm_model on codergen nodes")
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
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_model) == "claude-sonnet-4-6");
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
    // Only CodergenNode (box shape) gets llm_model stamped
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_model) == "box-model");
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
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_model) == "explicit-model");
}

SNITCH_TEST_CASE("[transform] apply_transforms with no stylesheet -- llm fields unchanged")
{
    auto g = make_valid_linear();
    auto out = apply_transforms(g);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_model).empty());
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
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->prompt) == "Plan: run tests");
    SNITCH_CHECK(type_safe::get(cn->llm_model) == "m1");
}

SNITCH_TEST_CASE("[transform] stylesheet reasoning_effort property applied to codergen nodes")
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
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_REQUIRE(cn->reasoning_effort.has_value());
    SNITCH_CHECK(*cn->reasoning_effort == ReasoningEffort::medium);
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
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_model) == "second");
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
    // "work" node (CodergenNode) with class "fast" gets "low" (higher specificity)
    const auto* cn_work = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn_work != nullptr);
    SNITCH_REQUIRE(cn_work->reasoning_effort.has_value());
    SNITCH_CHECK(*cn_work->reasoning_effort == ReasoningEffort::low);
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
    const auto* cn = find_codergen_node(out, "review");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_model) == "id-model");
}

SNITCH_TEST_CASE("[transform] stylesheet css_class multi-value whitespace trimmed -- 7.6-U-001")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet=".foo { llm_model: trimmed-model; }"]
            start [shape=Mdiamond]
            work  [shape=box, class=" foo , bar "]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_model) == "trimmed-model");
}

SNITCH_TEST_CASE("[transform] stylesheet unknown property key silently ignored -- 7.6-U-002")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { unknown_prop: skip_me; llm_model: valid-model; }"]
            start [shape=Mdiamond]
            work  [shape=box]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_model) == "valid-model");
}

SNITCH_TEST_CASE("[transform] stylesheet unknown reasoning_effort value silently ignored -- 7.6-U-003")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { reasoning_effort: bogus; }"]
            start [shape=Mdiamond]
            work  [shape=box]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(!cn->reasoning_effort.has_value());
}

SNITCH_TEST_CASE("[transform] stylesheet equal-specificity cross-property both apply -- 7.6-U-004")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { llm_model: m1; } * { llm_provider: p1; }"]
            start [shape=Mdiamond]
            work  [shape=box]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_model) == "m1");
    SNITCH_CHECK(type_safe::get(cn->llm_provider) == "p1");
}

SNITCH_TEST_CASE("[transform] stylesheet Mdiamond shape selector matches start node -- 7.6-U-005")
{
    // Mdiamond selector matches nodes with that shape; but llm_model only stamps CodergenNode.
    // Verify that a box-shaped CodergenNode is NOT matched by Mdiamond, so its llm_model stays empty.
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="Mdiamond { llm_model: diamond-model; }"]
            start [shape=Mdiamond]
            work  [shape=box]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    const auto* cn_work = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn_work != nullptr);
    SNITCH_CHECK(type_safe::get(cn_work->llm_model).empty());
}

SNITCH_TEST_CASE("[transform] stylesheet Msquare shape selector matches done node -- 7.6-U-006")
{
    // Msquare selector matches nodes with that shape; verify CodergenNode is not affected.
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="Msquare { llm_model: square-model; }"]
            start [shape=Mdiamond]
            work  [shape=box]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    const auto* cn_work = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn_work != nullptr);
    SNITCH_CHECK(type_safe::get(cn_work->llm_model).empty());
}

SNITCH_TEST_CASE("[transform] stylesheet llm_provider universal rule applies to codergen nodes -- 7.6-U-007")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="* { llm_provider: anthropic; }"]
            start [shape=Mdiamond]
            work  [shape=box]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_provider) == "anthropic");
}

SNITCH_TEST_CASE("[transform] stylesheet explicit llm_provider not overwritten by class selector -- 7.6-U-008")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet=".fast { llm_provider: class-provider; }"]
            start [shape=Mdiamond]
            work  [shape=box, class="fast", llm_provider="explicit-provider"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_provider) == "explicit-provider");
}

SNITCH_TEST_CASE("[transform] stylesheet explicit llm_provider not overwritten by ID selector -- 7.6-U-009")
{
    auto result = parse_graph(R"(
        digraph g {
            graph [model_stylesheet="#work { llm_provider: id-provider; }"]
            start [shape=Mdiamond]
            work  [shape=box, llm_provider="explicit-provider"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    StylesheetTransform xform;
    auto out = xform.apply(*result);
    const auto* cn = find_codergen_node(out, "work");
    SNITCH_REQUIRE(cn != nullptr);
    SNITCH_CHECK(type_safe::get(cn->llm_provider) == "explicit-provider");
}
