#include "attractor_test_support.hpp"
#include <attractor/dot_parser.hpp>
#include <attractor/graph.hpp>
#include <attractor/validator.hpp>

using namespace attractor;

// -- helpers --------------------------------------------------------------------

static Graph make_valid_linear()
{
    // start [shape=Mdiamond] -> work [shape=box, prompt="Do work"] -> done [shape=Msquare]
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

static auto count_by_rule(const std::vector<Diagnostic>& diags, std::string_view rule) -> int
{
    return static_cast<int>(std::count_if(diags.begin(), diags.end(),
                                          [&](const Diagnostic& d) { return type_safe::get(d.rule_id) == rule; }));
}

static auto has_error(const std::vector<Diagnostic>& diags) -> bool
{
    return std::any_of(diags.begin(), diags.end(), [](const Diagnostic& d) { return d.severity == Severity::error; });
}

// -- ERROR rules ----------------------------------------------------------------

// T01 / AC1 / AC5
SNITCH_TEST_CASE("[validator] start_node ERROR -- no start node")
{
    auto result = parse_graph(R"(digraph g { A [shape=box, prompt="x"]; B [shape=Msquare]; A -> B })");
    SNITCH_REQUIRE(result.has_value());
    auto diags = validate(*result);
    SNITCH_CHECK(count_by_rule(diags, "start_node") == 1);
    SNITCH_CHECK(diags.front().severity == Severity::error);
    SNITCH_CHECK(!type_safe::get(diags.front().message).empty());
    SNITCH_CHECK(!type_safe::get(diags.front().suggested_fix).empty());
}

// T02 / AC5
SNITCH_TEST_CASE("[validator] terminal_node ERROR -- no terminal node")
{
    auto result = parse_graph(R"(digraph g { start [shape=Mdiamond]; A [shape=box, prompt="x"]; start -> A })");
    SNITCH_REQUIRE(result.has_value());
    auto diags = validate(*result);
    SNITCH_CHECK(count_by_rule(diags, "terminal_node") == 1);
}

// T06 / AC5
SNITCH_TEST_CASE("[validator] edge_target_exists ERROR -- edge to nonexistent node")
{
    Graph g;
    {
        StartNode n;
        n.id = NodeId{"start"};
        n.shape = NodeShape::mdiamond;
        g.nodes.push_back(n);
    }
    {
        ExitNode n;
        n.id = NodeId{"done"};
        n.shape = NodeShape::msquare;
        g.nodes.push_back(n);
    }
    {
        Edge e;
        e.from = NodeId{"start"};
        e.to = NodeId{"ghost"};
        g.edges.push_back(e);
    }
    {
        Edge e;
        e.from = NodeId{"start"};
        e.to = NodeId{"done"};
        g.edges.push_back(e);
    }
    auto diags = validate(g);
    SNITCH_CHECK(count_by_rule(diags, "edge_target_exists") == 1);
}

// T07 / AC5
SNITCH_TEST_CASE("[validator] start_no_incoming ERROR")
{
    auto result = parse_graph(R"(
        digraph g {
            start [shape=Mdiamond]
            done  [shape=Msquare]
            A     [shape=box, prompt="x"]
            start -> A -> done
            A -> start
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    auto diags = validate(*result);
    SNITCH_CHECK(count_by_rule(diags, "start_no_incoming") == 1);
}

// T08 / AC5
SNITCH_TEST_CASE("[validator] exit_no_outgoing ERROR")
{
    auto result = parse_graph(R"(
        digraph g {
            start [shape=Mdiamond]
            done  [shape=Msquare]
            start -> done
            done -> start
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    auto diags = validate(*result);
    SNITCH_CHECK(count_by_rule(diags, "exit_no_outgoing") == 1);
}

// T09 / AC5
SNITCH_TEST_CASE("[validator] condition_syntax ERROR -- malformed condition")
{
    auto result = parse_graph(R"(
        digraph g {
            start [shape=Mdiamond]
            done  [shape=Msquare]
            start -> done [condition="badkeynooperator"]
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    auto diags = validate(*result);
    SNITCH_CHECK(count_by_rule(diags, "condition_syntax") == 1);
}

// T10 / AC5
SNITCH_TEST_CASE("[validator] condition_syntax passes for valid conditions")
{
    auto result = parse_graph(R"(
        digraph g {
            start [shape=Mdiamond]
            done  [shape=Msquare]
            start -> done [condition="outcome=success"]
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    auto diags = validate(*result);
    SNITCH_CHECK(count_by_rule(diags, "condition_syntax") == 0);
}

// T11 / AC5
SNITCH_TEST_CASE("[validator] stylesheet_syntax ERROR -- malformed stylesheet")
{
    Graph g = make_valid_linear();
    g.model_stylesheet = StylesheetId{"{ bad syntax without selector }"};
    auto diags = validate(g);
    SNITCH_CHECK(count_by_rule(diags, "stylesheet_syntax") == 1);
}

// T12 / AC5
SNITCH_TEST_CASE("[validator] stylesheet_syntax passes for valid stylesheet")
{
    Graph g = make_valid_linear();
    g.model_stylesheet = StylesheetId{"* { llm_model: claude-sonnet-4-6; }"};
    auto diags = validate(g);
    SNITCH_CHECK(count_by_rule(diags, "stylesheet_syntax") == 0);
}

// -- WARNING rules --------------------------------------------------------------

// T14 / AC2 / AC5
SNITCH_TEST_CASE("[validator] reachability WARNING -- orphan node")
{
    auto result = parse_graph(R"(
        digraph g {
            start  [shape=Mdiamond]
            work   [shape=box, prompt="work"]
            done   [shape=Msquare]
            orphan [shape=box, prompt="orphan"]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    auto diags = validate(*result);
    auto reach_diags = std::count_if(diags.begin(), diags.end(),
                                     [](const Diagnostic& d) { return type_safe::get(d.rule_id) == "reachability"; });
    SNITCH_CHECK(reach_diags == 1);
    auto& rd = *std::find_if(diags.begin(), diags.end(),
                             [](const Diagnostic& d) { return type_safe::get(d.rule_id) == "reachability"; });
    SNITCH_CHECK(rd.severity == Severity::warning);
    SNITCH_REQUIRE(rd.node_id.has_value());
    SNITCH_CHECK(type_safe::get(*rd.node_id) == "orphan");
}

// T15 / AC5
SNITCH_TEST_CASE("[validator] type_known WARNING -- unrecognised handler type")
{
    Graph g = make_valid_linear();
    std::get<CodergenNode>(g.nodes[1]).node_type = HandlerTypeName{"custom_handler"};

    // empty known_types -> no warning
    {
        auto diags = validate(g, {});
        SNITCH_CHECK(count_by_rule(diags, "type_known") == 0);
    }
    // known_types without custom_handler -> warning
    {
        ValidationConfig cfg{.known_types = {HandlerTypeName{"codergen"}}};
        auto diags = validate(g, cfg);
        SNITCH_CHECK(count_by_rule(diags, "type_known") == 1);
    }
    // known_types includes custom_handler -> no warning
    {
        ValidationConfig cfg{
            .known_types = {HandlerTypeName{"custom_handler"}, HandlerTypeName{"codergen"}}
        };
        auto diags = validate(g, cfg);
        SNITCH_CHECK(count_by_rule(diags, "type_known") == 0);
    }
}

// T16 / AC5
SNITCH_TEST_CASE("[validator] retry_target_exists WARNING -- missing retry target")
{
    Graph g = make_valid_linear();
    std::get<CodergenNode>(g.nodes[1]).retry_target = NodeId{"nonexistent_node"};
    auto diags = validate(g);
    SNITCH_CHECK(count_by_rule(diags, "retry_target_exists") >= 1);
}

// T / AC5
SNITCH_TEST_CASE("[validator] fidelity_valid WARNING -- no spurious diagnostic on valid graph")
{
    Graph g = make_valid_linear();
    auto diags = validate(g);
    SNITCH_CHECK(count_by_rule(diags, "fidelity_valid") == 0);
}

// T / AC5
SNITCH_TEST_CASE("[validator] fidelity_valid WARNING fires for out-of-range FidelityMode -- 4.4-U-005")
{
    Graph g = make_valid_linear();
    std::get<CodergenNode>(g.nodes[1]).fidelity = static_cast<FidelityMode>(99);
    auto diags = validate(g);
    SNITCH_CHECK(count_by_rule(diags, "fidelity_valid") >= 1);
}

// T17 / AC5
SNITCH_TEST_CASE("[validator] goal_gate_has_retry WARNING -- goal_gate without retry_target")
{
    Graph g = make_valid_linear();
    auto work_it =
        std::find_if(g.nodes.begin(), g.nodes.end(), [](const NodeVariant& nv) {
            return type_safe::get(to_base(nv).id) == "work";
        });
    SNITCH_REQUIRE(work_it != g.nodes.end());
    std::get<CodergenNode>(*work_it).goal_gate = ts::boolean{true};

    auto diags = validate(g);
    SNITCH_CHECK(count_by_rule(diags, "goal_gate_has_retry") == 1);

    g.retry_target = NodeId{"work"};
    auto diags2 = validate(g);
    SNITCH_CHECK(count_by_rule(diags2, "goal_gate_has_retry") == 0);
}

// T18 / AC5
SNITCH_TEST_CASE("[validator] prompt_on_llm_nodes WARNING -- codergen without prompt or label")
{
    auto result = parse_graph(R"(
        digraph g {
            start [shape=Mdiamond]
            work  [shape=box]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    auto diags = validate(*result);
    SNITCH_CHECK(count_by_rule(diags, "prompt_on_llm_nodes") == 1);
}

// -- AC3: fully valid graph -> no ERRORs ----------------------------------------

// T03 / AC3
SNITCH_TEST_CASE("[validator] valid linear graph produces no ERROR diagnostics")
{
    Graph g = make_valid_linear();
    auto diags = validate(g);
    SNITCH_CHECK(!has_error(diags));
}

// -- AC4: custom LintRule -------------------------------------------------------

// T13 / AC4
SNITCH_TEST_CASE("[validator] custom LintRule runs after built-in rules")
{
    struct AlwaysWarn final : public LintRule {
        [[nodiscard]] auto apply(const Graph&) const -> std::vector<Diagnostic> override
        {
            return {
                {.rule_id = RuleId{"custom_rule"},
                 .severity = Severity::warning,
                 .message = DiagnosticMessage{"Custom rule fired"}}
            };
        }
    };

    AlwaysWarn custom;
    Graph g = make_valid_linear();
    auto diags = validate(g, {}, {&custom});
    SNITCH_CHECK(count_by_rule(diags, "custom_rule") == 1);
}

// -- validate_or_raise ---------------------------------------------------------

// T04 / AC1
SNITCH_TEST_CASE("[validator] validate_or_raise throws ValidationError on error")
{
    auto result = parse_graph(R"(digraph g { A [shape=box, prompt="x"]; B [shape=Msquare]; A -> B })");
    SNITCH_REQUIRE(result.has_value());
    bool threw = false;
    try {
        validate_or_raise(*result);
    }
    catch (const ValidationError& e) {
        threw = true;
        SNITCH_CHECK(!e.diagnostics().empty());
        SNITCH_CHECK(std::all_of(e.diagnostics().begin(), e.diagnostics().end(),
                                 [](const Diagnostic& d) { return d.severity == Severity::error; }));
    }
    SNITCH_CHECK(threw);
}

// T05 / AC3
SNITCH_TEST_CASE("[validator] validate_or_raise does not throw for valid graph")
{
    Graph g = make_valid_linear();
    SNITCH_CHECK_NOTHROW(validate_or_raise(g));
}
