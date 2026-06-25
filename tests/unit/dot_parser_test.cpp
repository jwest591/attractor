#include "attractor_test_support.hpp"
#include <attractor/dot_parser.hpp>
#include <attractor/graph.hpp>
#include <chrono>
#include <variant>

using namespace attractor;

// -- AC1: Linear pipeline ------------------------------------------------------

SNITCH_TEST_CASE("[dot_parser] linear pipeline parses correctly")
{
    const auto* source = R"(
        digraph mypipe {
            graph [goal="Test goal", label="My Pipeline"]
            start [shape=Mdiamond, label="Start"]
            work  [shape=box, prompt="Do work"]
            done  [shape=Msquare, label="Done"]
            start -> work -> done
        }
    )";
    auto result = parse_graph(source);
    SNITCH_REQUIRE(result.has_value());
    const auto& g = *result;
    SNITCH_CHECK(g.nodes.size() == 3u);
    SNITCH_CHECK(g.edges.size() == 2u);
    SNITCH_CHECK(type_safe::get(g.goal) == "Test goal");
    SNITCH_CHECK(type_safe::get(g.label) == "My Pipeline");
}

// -- AC2: Chained edges --------------------------------------------------------

SNITCH_TEST_CASE("[dot_parser] chained edges produce individual edges")
{
    auto result = parse_graph("digraph g { A -> B -> C }");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(result->edges.size() == 2u);
    SNITCH_CHECK(type_safe::get(result->edges[0].from) == "A");
    SNITCH_CHECK(type_safe::get(result->edges[0].to) == "B");
    SNITCH_CHECK(type_safe::get(result->edges[1].from) == "B");
    SNITCH_CHECK(type_safe::get(result->edges[1].to) == "C");
}

// -- AC3: Subgraph flattening and class derivation -----------------------------

SNITCH_TEST_CASE("[dot_parser] subgraph class derived from label")
{
    auto result = parse_graph(R"(
        digraph g {
            subgraph cluster_loop {
                label = "Loop A"
                node [shape=box]
                MyNode [label="Test"]
            }
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_REQUIRE(!result->nodes.empty());
    SNITCH_CHECK(type_safe::get(to_base(result->nodes[0]).css_class).find("loop-a") != std::string::npos);
}

// -- AC4: Comments stripped ----------------------------------------------------

SNITCH_TEST_CASE("[dot_parser] comments stripped")
{
    auto result = parse_graph(R"(
        digraph g {
            // this is a comment
            A /* inline comment */ -> B
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(result->edges.size() == 1u);
}

SNITCH_TEST_CASE("[dot_parser] block comment does not strip quoted string content")
{
    auto result = parse_graph(R"(
        digraph g {
            A [label="// not a comment"]
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_REQUIRE(!result->nodes.empty());
    SNITCH_CHECK(type_safe::get(to_base(result->nodes[0]).label) == "// not a comment");
}

// -- AC5: Rejected inputs ------------------------------------------------------

SNITCH_TEST_CASE("[dot_parser] strict digraph rejected")
{
    auto result = parse_graph("strict digraph g { A -> B }");
    SNITCH_REQUIRE(!result.has_value());
    SNITCH_CHECK(!result.error().message.empty());
}

SNITCH_TEST_CASE("[dot_parser] undirected edge rejected")
{
    auto result = parse_graph("digraph g { A -- B }");
    SNITCH_REQUIRE(!result.has_value());
}

SNITCH_TEST_CASE("[dot_parser] HTML label rejected")
{
    auto result = parse_graph("digraph g { A [label=<b>HTML</b>] }");
    SNITCH_REQUIRE(!result.has_value());
}

// -- AC6: Invalid node IDs -----------------------------------------------------

SNITCH_TEST_CASE("[dot_parser] invalid node ID rejected")
{
    // Digit-prefixed ID: 123node lexes as integer "123" then identifier "node"
    // Parser sees integer token in statement position -> unexpected -> ParseError
    auto r1 = parse_graph("digraph g { 123node [label=\"x\"] }");
    SNITCH_REQUIRE(!r1.has_value());

    // ID with dash: node-id lexes as bare_value -> parser rejects with descriptive message
    auto r2 = parse_graph("digraph g { node-id [label=\"x\"] }");
    SNITCH_REQUIRE(!r2.has_value());
    SNITCH_CHECK(!r2.error().message.empty());

    // Underscore-prefixed and alphanumeric IDs are valid
    auto r3 = parse_graph("digraph g { node_a -> _node2 }");
    SNITCH_REQUIRE(r3.has_value());
}

// -- AC7: Duration parsing -----------------------------------------------------

SNITCH_TEST_CASE("[dot_parser] timeout duration parsing")
{
    using namespace std::chrono;
    auto result = parse_graph(R"(
        digraph g {
            A [shape=box, timeout="900s"]
            B [shape=box, timeout="15m"]
            C [shape=box, timeout="250ms"]
            D [shape=box, timeout="2h"]
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    const auto& nodes = result->nodes;
    auto find_node = [&](std::string_view id) -> const Node* {
        for (const auto& nv : nodes) {
            const Node& n = to_base(nv);
            if (type_safe::get(n.id) == id) {
                return &n;
            }
        }
        return nullptr;
    };
    auto* a = find_node("A");
    SNITCH_REQUIRE(a != nullptr);
    SNITCH_REQUIRE(a->timeout.has_value());
    SNITCH_CHECK(a->timeout->get_value() == milliseconds{900'000});

    auto* b = find_node("B");
    SNITCH_REQUIRE(b != nullptr);
    SNITCH_REQUIRE(b->timeout.has_value());
    SNITCH_CHECK(b->timeout->get_value() == milliseconds{900'000});

    auto* c = find_node("C");
    SNITCH_REQUIRE(c != nullptr);
    SNITCH_REQUIRE(c->timeout.has_value());
    SNITCH_CHECK(c->timeout->get_value() == milliseconds{250});

    auto* d = find_node("D");
    SNITCH_REQUIRE(d != nullptr);
    SNITCH_REQUIRE(d->timeout.has_value());
    SNITCH_CHECK(d->timeout->get_value() == milliseconds{7'200'000});
}

// -- AC8: Subgraph defaults inheritance ---------------------------------------

SNITCH_TEST_CASE("[dot_parser] subgraph defaults inheritance")
{
    auto result = parse_graph(R"(
        digraph g {
            subgraph cluster_loop {
                label = "Loop A"
                node [thread_id="loop-a", timeout="900s"]
                Plan      [label="Plan next step"]
                Implement [label="Implement", timeout="1800s"]
            }
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    const auto& nodes = result->nodes;
    auto find = [&](std::string_view id) -> const Node* {
        for (const auto& nv : nodes) {
            const Node& n = to_base(nv);
            if (type_safe::get(n.id) == id) {
                return &n;
            }
        }
        return nullptr;
    };
    using namespace std::chrono;

    auto* plan = find("Plan");
    SNITCH_REQUIRE(plan != nullptr);
    SNITCH_REQUIRE(plan->thread_id.has_value());
    SNITCH_CHECK(type_safe::get(*plan->thread_id) == "loop-a");
    SNITCH_REQUIRE(plan->timeout.has_value());
    SNITCH_CHECK(plan->timeout->get_value() == milliseconds{900'000});

    auto* impl = find("Implement");
    SNITCH_REQUIRE(impl != nullptr);
    SNITCH_REQUIRE(impl->thread_id.has_value());
    SNITCH_CHECK(type_safe::get(*impl->thread_id) == "loop-a");
    SNITCH_REQUIRE(impl->timeout.has_value());
    SNITCH_CHECK(impl->timeout->get_value() == milliseconds{1'800'000});
}

// -- DoD 11.1: Additional coverage -------------------------------------------

SNITCH_TEST_CASE("[dot_parser] graph-level attributes")
{
    auto result = parse_graph(R"(
        digraph pipeline {
            graph [goal="Automate the thing", model_stylesheet="minimal", default_max_retries=3]
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(result->goal) == "Automate the thing");
    SNITCH_CHECK(type_safe::get(result->model_stylesheet) == "minimal");
    SNITCH_CHECK(result->default_max_retries.get_value() == 3);
    SNITCH_CHECK(type_safe::get(result->digraph_id) == "pipeline");
}

SNITCH_TEST_CASE("[dot_parser] edge attributes")
{
    auto result = parse_graph(R"(
        digraph g {
            A -> B [label="next", condition="outcome=success", weight=5]
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_REQUIRE(result->edges.size() == 1u);
    const auto& e = result->edges[0];
    SNITCH_CHECK(type_safe::get(e.label) == "next");
    SNITCH_CHECK(type_safe::get(e.condition) == "outcome=success");
    SNITCH_CHECK(e.weight.get_value() == 5);
}

SNITCH_TEST_CASE("[dot_parser] node and edge defaults scoped correctly")
{
    auto result = parse_graph(R"(
        digraph g {
            node [shape=diamond]
            A
            B
            subgraph s {
                node [shape=box]
                C
            }
            D
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    auto find = [&](std::string_view id) -> const Node* {
        for (const auto& nv : result->nodes) {
            const Node& n = to_base(nv);
            if (type_safe::get(n.id) == id) {
                return &n;
            }
        }
        return nullptr;
    };
    auto* a = find("A");
    SNITCH_REQUIRE(a != nullptr);
    SNITCH_CHECK(a->shape == NodeShape::diamond);

    auto* c = find("C");
    SNITCH_REQUIRE(c != nullptr);
    SNITCH_CHECK(c->shape == NodeShape::box);

    // D is after subgraph -- defaults should revert to diamond
    auto* d = find("D");
    SNITCH_REQUIRE(d != nullptr);
    SNITCH_CHECK(d->shape == NodeShape::diamond);
}

SNITCH_TEST_CASE("[dot_parser] multi-line attribute block")
{
    auto result = parse_graph(R"(
        digraph g {
            A [
                shape=box,
                label="Multi-line node",
                goal_gate=true
            ]
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_REQUIRE(!result->nodes.empty());
    const Node& n = to_base(result->nodes[0]);
    SNITCH_CHECK(type_safe::get(n.label) == "Multi-line node");
    SNITCH_CHECK(n.goal_gate == true);
}

SNITCH_TEST_CASE("[dot_parser] class attribute applied directly to node")
{
    auto result = parse_graph(R"(
        digraph g {
            A [class="my-class"]
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_REQUIRE(!result->nodes.empty());
    SNITCH_CHECK(type_safe::get(to_base(result->nodes[0]).css_class) == "my-class");
}

SNITCH_TEST_CASE("[dot_parser] class attribute appended to subgraph css class")
{
    auto result = parse_graph(R"(
        digraph g {
            subgraph cluster_a {
                label = "Group A"
                A [class="extra"]
            }
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_REQUIRE(!result->nodes.empty());
    SNITCH_CHECK(type_safe::get(to_base(result->nodes[0]).css_class).find("extra") != std::string::npos);
    SNITCH_CHECK(type_safe::get(to_base(result->nodes[0]).css_class).find("group-a") != std::string::npos);
}

SNITCH_TEST_CASE("[dot_parser] node auto-created from edge endpoint")
{
    auto result = parse_graph("digraph g { A -> B }");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(result->nodes.size() == 2u);
    SNITCH_CHECK(result->edges.size() == 1u);
}

SNITCH_TEST_CASE("[dot_parser] quoted and unquoted attribute values")
{
    auto result = parse_graph(R"(
        digraph g {
            A [shape=box, goal_gate=true, label="quoted label"]
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_REQUIRE(!result->nodes.empty());
    const Node& n = to_base(result->nodes[0]);
    SNITCH_CHECK(n.shape == NodeShape::box);
    SNITCH_CHECK(n.goal_gate == true);
    SNITCH_CHECK(type_safe::get(n.label) == "quoted label");
}

SNITCH_TEST_CASE("[dot_parser] unknown shape emits ParseError")
{
    auto result = parse_graph(R"(
        digraph g {
            A [shape=unknownshape]
        }
    )");
    SNITCH_REQUIRE(!result.has_value());
    SNITCH_CHECK(result.error().message.find("unknownshape") != std::string::npos);
}

SNITCH_TEST_CASE("[dot_parser] all 9 canonical shapes parse to correct enum values")
{
    auto result = parse_graph(R"(
        digraph g {
            S  [shape=Mdiamond]
            X  [shape=Msquare]
            B  [shape=box]
            H  [shape=hexagon]
            D  [shape=diamond]
            C  [shape=component]
            T  [shape=tripleoctagon]
            P  [shape=parallelogram]
            M  [shape=house]
        }
    )");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_REQUIRE(result->nodes.size() == 9);

    auto find = [&](std::string_view id) -> const Node* {
        for (const auto& nv : result->nodes) {
            const Node& n = to_base(nv);
            if (type_safe::get(n.id) == id) return &n;
        }
        return nullptr;
    };

    auto* s = find("S"); SNITCH_REQUIRE(s); SNITCH_CHECK(s->shape == NodeShape::mdiamond);
    auto* x = find("X"); SNITCH_REQUIRE(x); SNITCH_CHECK(x->shape == NodeShape::msquare);
    auto* b = find("B"); SNITCH_REQUIRE(b); SNITCH_CHECK(b->shape == NodeShape::box);
    auto* h = find("H"); SNITCH_REQUIRE(h); SNITCH_CHECK(h->shape == NodeShape::hexagon);
    auto* d = find("D"); SNITCH_REQUIRE(d); SNITCH_CHECK(d->shape == NodeShape::diamond);
    auto* c = find("C"); SNITCH_REQUIRE(c); SNITCH_CHECK(c->shape == NodeShape::component);
    auto* t = find("T"); SNITCH_REQUIRE(t); SNITCH_CHECK(t->shape == NodeShape::triple_octagon);
    auto* p = find("P"); SNITCH_REQUIRE(p); SNITCH_CHECK(p->shape == NodeShape::parallelogram);
    auto* m = find("M"); SNITCH_REQUIRE(m); SNITCH_CHECK(m->shape == NodeShape::house);
}

// -- Story 7.13: Parser constructs correct NodeVariant per shape (AC5) -----------

using namespace attractor::test;

SNITCH_TEST_CASE("[dot_parser] box shape creates CodergenNode in NodeVariant -- 7.13-U-015")
{
    auto g = parse_ok(R"(digraph g { work [shape=box, prompt="Do work"] })");
    SNITCH_REQUIRE(g.nodes.size() == 1u);
    SNITCH_REQUIRE(std::holds_alternative<CodergenNode>(g.nodes[0]));
    const auto& n = std::get<CodergenNode>(g.nodes[0]);
    SNITCH_CHECK(type_safe::get(n.prompt) == "Do work");
}

SNITCH_TEST_CASE("[dot_parser] mdiamond shape creates StartNode -- 7.13-U-016")
{
    auto g = parse_ok(R"(digraph g { s [shape=Mdiamond, label="Start"] })");
    SNITCH_REQUIRE(g.nodes.size() == 1u);
    SNITCH_CHECK(std::holds_alternative<StartNode>(g.nodes[0]));
    const auto& n = std::get<StartNode>(g.nodes[0]);
    SNITCH_CHECK(type_safe::get(n.label) == "Start");
}

SNITCH_TEST_CASE("[dot_parser] msquare shape creates ExitNode -- 7.13-U-017")
{
    auto g = parse_ok(R"(digraph g { e [shape=Msquare] })");
    SNITCH_REQUIRE(g.nodes.size() == 1u);
    SNITCH_CHECK(std::holds_alternative<ExitNode>(g.nodes[0]));
}

SNITCH_TEST_CASE("[dot_parser] parallelogram shape creates ToolNode with tool_command -- 7.13-U-018")
{
    auto g = parse_ok(R"(digraph g { t [shape=parallelogram, tool_command="echo hello"] })");
    SNITCH_REQUIRE(g.nodes.size() == 1u);
    SNITCH_REQUIRE(std::holds_alternative<ToolNode>(g.nodes[0]));
    const auto& n = std::get<ToolNode>(g.nodes[0]);
    SNITCH_CHECK(type_safe::get(n.tool_command) == "echo hello");
}

SNITCH_TEST_CASE("[dot_parser] house shape creates ManagerNode -- 7.13-U-019")
{
    auto g = parse_ok(R"(digraph g { m [shape=house] })");
    SNITCH_REQUIRE(g.nodes.size() == 1u);
    SNITCH_CHECK(std::holds_alternative<ManagerNode>(g.nodes[0]));
}

SNITCH_TEST_CASE("[dot_parser] hexagon shape creates WaitHumanNode -- 7.13-U-020")
{
    auto g = parse_ok(R"(digraph g { w [shape=hexagon] })");
    SNITCH_REQUIRE(g.nodes.size() == 1u);
    SNITCH_CHECK(std::holds_alternative<WaitHumanNode>(g.nodes[0]));
}

SNITCH_TEST_CASE("[dot_parser] component shape creates ParallelNode -- 7.13-U-021")
{
    auto g = parse_ok(R"(digraph g { p [shape=component] })");
    SNITCH_REQUIRE(g.nodes.size() == 1u);
    SNITCH_CHECK(std::holds_alternative<ParallelNode>(g.nodes[0]));
}

SNITCH_TEST_CASE("[dot_parser] triple_octagon shape creates FanInNode with prompt -- 7.13-U-022")
{
    auto g = parse_ok(R"(digraph g { f [shape=tripleoctagon, prompt="Merge results"] })");
    SNITCH_REQUIRE(g.nodes.size() == 1u);
    SNITCH_REQUIRE(std::holds_alternative<FanInNode>(g.nodes[0]));
    const auto& n = std::get<FanInNode>(g.nodes[0]);
    SNITCH_CHECK(type_safe::get(n.prompt) == "Merge results");
}

SNITCH_TEST_CASE("[dot_parser] diamond shape creates ConditionalNode -- 7.13-U-023")
{
    auto g = parse_ok(R"(digraph g { c [shape=diamond] })");
    SNITCH_REQUIRE(g.nodes.size() == 1u);
    SNITCH_CHECK(std::holds_alternative<ConditionalNode>(g.nodes[0]));
}

SNITCH_TEST_CASE("[dot_parser] subgraph stamps enclosing_subgraph on contained nodes -- 7.13-U-024")
{
    auto g = parse_ok(R"(
        digraph g {
            subgraph cluster_loop {
                label = "Loop Stage"
                work [shape=box, prompt="Do work"]
            }
        }
    )");
    SNITCH_REQUIRE(!g.nodes.empty());
    // safe: all derived types inherit from Node, lambda upcast works for any alternative
    const Node& base = std::visit([](const Node& n) -> const Node& { return n; }, g.nodes[0]);
    SNITCH_REQUIRE(base.enclosing_subgraph.has_value());
    SNITCH_CHECK(!base.enclosing_subgraph->empty());
}

// -- default_thread_id graph attribute (Story 7.4 revised: resolve_thread_key Correctness) --

SNITCH_TEST_CASE("[dot_parser] default_thread_id graph attribute parsed correctly -- 7.4-U-007")
{
    const auto result = parse_ok(R"(
        digraph g {
            graph [default_thread_id="shared-loop"]
            start [shape=Mdiamond]
            work [shape=box]
            exit [shape=Msquare]
            start -> work -> exit
        }
    )");
    SNITCH_REQUIRE(result.default_thread_id.has_value());
    SNITCH_CHECK(type_safe::get(*result.default_thread_id) == "shared-loop");
}
