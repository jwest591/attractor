#include "attractor_test_support.hpp"
#include <attractor/graph.hpp>
#include <chrono>
#include <nlohmann/json.hpp>

using namespace attractor;

SNITCH_TEST_CASE("[graph_model] Node default values")
{
    Node n;
    SNITCH_CHECK(n.shape == NodeShape::box);
    SNITCH_CHECK(!n.goal_gate);
    SNITCH_CHECK(!n.auto_status);
    SNITCH_CHECK(!n.allow_partial);
    SNITCH_CHECK(!n.max_retries.has_value());
    SNITCH_CHECK(!n.timeout.has_value());
    SNITCH_CHECK(!n.fidelity.has_value());
    SNITCH_CHECK(!n.thread_id.has_value());
}

SNITCH_TEST_CASE("[graph_model] Edge default values")
{
    Edge e;
    SNITCH_CHECK(e.weight.get_value() == 0);
    SNITCH_CHECK(!e.loop_restart);
    SNITCH_CHECK(!e.fidelity.has_value());
    SNITCH_CHECK(!e.thread_id.has_value());
}

SNITCH_TEST_CASE("[graph_model] Graph default values")
{
    Graph g;
    SNITCH_CHECK(g.default_max_retries.get_value() == 0);
    SNITCH_CHECK(!g.default_fidelity.has_value());
}

SNITCH_TEST_CASE("[graph_model] Node JSON round-trip")
{
    using namespace std::chrono;
    Node n;
    n.id = NodeId{"node_a"};
    n.label = NodeLabel{"Node A"};
    n.shape = NodeShape::box;
    n.goal_gate = true;
    n.timeout = TimeoutDuration{milliseconds{5000}};

    nlohmann::json j;
    to_json(j, n);

    Node restored;
    from_json(j, restored);
    SNITCH_CHECK(restored.id == n.id);
    SNITCH_CHECK(restored.label == n.label);
    SNITCH_CHECK(restored.shape == NodeShape::box);
    SNITCH_CHECK(restored.goal_gate == n.goal_gate);
    SNITCH_REQUIRE(restored.timeout.has_value());
    SNITCH_CHECK(restored.timeout->get_value() == milliseconds{5000});
}

SNITCH_TEST_CASE("[graph_model] Edge JSON round-trip")
{
    Edge e;
    e.from = NodeId{"A"};
    e.to = NodeId{"B"};
    e.label = EdgeLabel{"next"};
    e.weight = Weight{3};
    e.loop_restart = true;

    nlohmann::json j;
    to_json(j, e);

    Edge restored;
    from_json(j, restored);
    SNITCH_CHECK(restored.from == e.from);
    SNITCH_CHECK(restored.to == e.to);
    SNITCH_CHECK(restored.label == e.label);
    SNITCH_CHECK(restored.weight.get_value() == 3);
    SNITCH_CHECK(restored.loop_restart == true);
}

SNITCH_TEST_CASE("[graph_model] Graph JSON round-trip")
{
    Graph g;
    g.digraph_id = GraphId{"mypipe"};
    g.goal = GoalText{"Test goal"};
    g.label = GraphLabel{"My Pipeline"};

    StartNode start;
    start.id = NodeId{"start"};
    start.label = NodeLabel{"Start"};
    start.shape = NodeShape::mdiamond;
    g.nodes.push_back(start);

    ExitNode end;
    end.id = NodeId{"end"};
    end.label = NodeLabel{"End"};
    end.shape = NodeShape::msquare;
    g.nodes.push_back(end);

    Edge e;
    e.from = NodeId{"start"};
    e.to = NodeId{"end"};
    g.edges.push_back(e);

    nlohmann::json j;
    to_json(j, g);

    Graph restored;
    from_json(j, restored);
    SNITCH_CHECK(type_safe::get(restored.digraph_id) == "mypipe");
    SNITCH_CHECK(type_safe::get(restored.goal) == "Test goal");
    SNITCH_CHECK(restored.nodes.size() == 2u);
    SNITCH_CHECK(restored.edges.size() == 1u);
}

SNITCH_TEST_CASE("[graph_model] TimeoutDuration storage and retrieval")
{
    using namespace std::chrono;
    Node n;
    n.timeout = TimeoutDuration{milliseconds{900'000}};
    SNITCH_REQUIRE(n.timeout.has_value());
    SNITCH_CHECK(n.timeout->get_value() == milliseconds{900'000});
}

SNITCH_TEST_CASE("[graph_model] Graph::default_thread_id JSON round-trip with value -- 7.4-U-008")
{
    Graph g;
    g.default_thread_id = ThreadId{"my-thread"};
    nlohmann::json j;
    to_json(j, g);
    Graph restored;
    from_json(j, restored);
    SNITCH_REQUIRE(restored.default_thread_id.has_value());
    SNITCH_CHECK(type_safe::get(*restored.default_thread_id) == "my-thread");
}

SNITCH_TEST_CASE("[graph_model] Graph::default_thread_id JSON round-trip absent yields nullopt -- 7.4-U-009")
{
    Graph g;
    nlohmann::json j;
    to_json(j, g);
    Graph restored;
    from_json(j, restored);
    SNITCH_CHECK(!restored.default_thread_id.has_value());
}
