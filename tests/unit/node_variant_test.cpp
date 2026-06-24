#include "attractor_test_support.hpp"
#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <nlohmann/json.hpp>
#include <variant>

using namespace attractor;
using namespace attractor::test;

// -- AC4: SubgraphId strong typedef -----------------------------------------------

static_assert(!std::is_assignable_v<SubgraphId&, std::string>,
              "SubgraphId must not be directly assignable from std::string");

SNITCH_TEST_CASE("[types] SubgraphId construction equality and ordering -- 7.13-U-001")
{
    SubgraphId a{"cluster_loop"};
    SubgraphId b{"cluster_plan"};
    SubgraphId a2{"cluster_loop"};

    SNITCH_REQUIRE(a == a2);
    SNITCH_REQUIRE(a != b);
    SNITCH_REQUIRE(a < b);
}

SNITCH_TEST_CASE("[types] SubgraphId empty returns true for empty string -- 7.13-U-002")
{
    SubgraphId empty_id{""};
    SNITCH_CHECK(empty_id.empty());
    SubgraphId non_empty{"cluster_x"};
    SNITCH_CHECK(!non_empty.empty());
}

SNITCH_TEST_CASE("[types] SubgraphId JSON round-trip -- 7.13-U-003")
{
    SubgraphId original{"cluster_loop"};
    nlohmann::json j;
    to_json(j, original);
    SNITCH_REQUIRE(j.get<std::string>() == "cluster_loop");
    SubgraphId restored{""};
    from_json(j, restored);
    SNITCH_REQUIRE(original == restored);
}

// -- AC1: Node base carries enclosing_subgraph ------------------------------------

SNITCH_TEST_CASE("[graph_model] Node base carries optional enclosing_subgraph field -- 7.13-U-004")
{
    Node n;
    SNITCH_CHECK(!n.enclosing_subgraph.has_value());
    n.enclosing_subgraph = SubgraphId{"cluster_loop"};
    SNITCH_REQUIRE(n.enclosing_subgraph.has_value());
    SNITCH_CHECK(*n.enclosing_subgraph == SubgraphId{"cluster_loop"});
}

// -- AC2: Nine derived node structs with correct fields ---------------------------

SNITCH_TEST_CASE("[graph_model] CodergenNode carries prompt llm_model llm_provider and optional reasoning_effort -- 7.13-U-005")
{
    CodergenNode n;
    n.id = NodeId{"work"};
    n.prompt = PromptText{"Do the work"};
    n.llm_model = LlmModel{"claude-sonnet-4-6"};
    n.llm_provider = LlmProvider{"anthropic"};
    SNITCH_CHECK(!n.reasoning_effort.has_value());
    SNITCH_CHECK(type_safe::get(n.prompt) == "Do the work");
    SNITCH_CHECK(type_safe::get(n.llm_model) == "claude-sonnet-4-6");
}

SNITCH_TEST_CASE("[graph_model] ToolNode carries tool_command -- 7.13-U-006")
{
    ToolNode n;
    n.id = NodeId{"run_tests"};
    n.tool_command = ShellCommand{"make test"};
    SNITCH_CHECK(type_safe::get(n.tool_command) == "make test");
}

SNITCH_TEST_CASE("[graph_model] ManagerNode carries manager_stop_condition and manager_max_cycles -- 7.13-U-007")
{
    ManagerNode n;
    n.manager_stop_condition = ConditionExpr{"ctx.done == true"};
    SNITCH_CHECK(n.manager_max_cycles == 1000);
    n.manager_max_cycles = 5;
    SNITCH_CHECK(n.manager_max_cycles == 5);
}

SNITCH_TEST_CASE("[graph_model] WaitHumanNode carries human_default_choice -- 7.13-U-008")
{
    WaitHumanNode n;
    n.human_default_choice = NodeId{"proceed"};
    SNITCH_CHECK(type_safe::get(n.human_default_choice) == "proceed");
}

SNITCH_TEST_CASE("[graph_model] ParallelNode carries max_parallel and join_policy with correct defaults -- 7.13-U-009")
{
    ParallelNode n;
    SNITCH_CHECK(n.max_parallel.get_value() == 4);
    SNITCH_CHECK(n.join_policy == JoinPolicy::wait_all);
}

SNITCH_TEST_CASE("[graph_model] FanInNode carries prompt field -- 7.13-U-010")
{
    FanInNode n;
    n.prompt = PromptText{"Synthesize branch results"};
    SNITCH_CHECK(type_safe::get(n.prompt) == "Synthesize branch results");
}

// -- AC3: NodeVariant alias and Graph::nodes --------------------------------------

SNITCH_TEST_CASE("[graph_model] NodeVariant holds all 9 derived node types -- 7.13-U-011")
{
    static_assert(
        std::is_same_v<NodeVariant,
                       std::variant<StartNode, ExitNode, CodergenNode, ToolNode, ManagerNode,
                                    WaitHumanNode, ParallelNode, FanInNode, ConditionalNode>>,
        "NodeVariant must be exactly the 9-type variant");

    NodeVariant v = CodergenNode{};
    SNITCH_CHECK(std::holds_alternative<CodergenNode>(v));
    v = StartNode{};
    SNITCH_CHECK(std::holds_alternative<StartNode>(v));
    v = ExitNode{};
    SNITCH_CHECK(std::holds_alternative<ExitNode>(v));
    v = ToolNode{};
    SNITCH_CHECK(std::holds_alternative<ToolNode>(v));
    v = ManagerNode{};
    SNITCH_CHECK(std::holds_alternative<ManagerNode>(v));
    v = WaitHumanNode{};
    SNITCH_CHECK(std::holds_alternative<WaitHumanNode>(v));
    v = ParallelNode{};
    SNITCH_CHECK(std::holds_alternative<ParallelNode>(v));
    v = FanInNode{};
    SNITCH_CHECK(std::holds_alternative<FanInNode>(v));
    v = ConditionalNode{};
    SNITCH_CHECK(std::holds_alternative<ConditionalNode>(v));
}

SNITCH_TEST_CASE("[graph_model] Graph nodes vector stores NodeVariant elements -- 7.13-U-012")
{
    Graph g;
    CodergenNode work;
    work.id = NodeId{"work"};
    g.nodes.push_back(work);

    StartNode start;
    start.id = NodeId{"start"};
    g.nodes.push_back(start);

    SNITCH_REQUIRE(g.nodes.size() == 2u);
    SNITCH_CHECK(std::holds_alternative<CodergenNode>(g.nodes[0]));
    SNITCH_CHECK(std::holds_alternative<StartNode>(g.nodes[1]));
}

// -- AC9: NodeVariant JSON serialization ------------------------------------------

SNITCH_TEST_CASE("[graph_model] NodeVariant JSON round-trip preserves CodergenNode type and fields -- 7.13-U-013")
{
    CodergenNode src;
    src.id = NodeId{"work"};
    src.shape = NodeShape::box;
    src.prompt = PromptText{"Do the thing"};
    src.llm_model = LlmModel{"claude-sonnet-4-6"};

    NodeVariant v = src;
    nlohmann::json j;
    to_json(j, v);

    NodeVariant restored;
    from_json(j, restored);
    SNITCH_REQUIRE(std::holds_alternative<CodergenNode>(restored));
    const auto& r = std::get<CodergenNode>(restored);
    SNITCH_CHECK(r.id == src.id);
    SNITCH_CHECK(r.prompt == src.prompt);
    SNITCH_CHECK(r.llm_model == src.llm_model);
}

SNITCH_TEST_CASE("[graph_model] from_json NodeVariant uses shape as discriminator for all 9 types -- 7.13-U-014")
{
    auto make_j = [](const char* shape_str) {
        nlohmann::json j;
        j["id"] = "n";
        j["shape"] = shape_str;
        return j;
    };

    NodeVariant v;

    from_json(make_j("Mdiamond"), v);
    SNITCH_CHECK(std::holds_alternative<StartNode>(v));

    from_json(make_j("Msquare"), v);
    SNITCH_CHECK(std::holds_alternative<ExitNode>(v));

    from_json(make_j("box"), v);
    SNITCH_CHECK(std::holds_alternative<CodergenNode>(v));

    from_json(make_j("parallelogram"), v);
    SNITCH_CHECK(std::holds_alternative<ToolNode>(v));

    from_json(make_j("house"), v);
    SNITCH_CHECK(std::holds_alternative<ManagerNode>(v));

    from_json(make_j("hexagon"), v);
    SNITCH_CHECK(std::holds_alternative<WaitHumanNode>(v));

    from_json(make_j("component"), v);
    SNITCH_CHECK(std::holds_alternative<ParallelNode>(v));

    from_json(make_j("triple_octagon"), v);
    SNITCH_CHECK(std::holds_alternative<FanInNode>(v));

    from_json(make_j("diamond"), v);
    SNITCH_CHECK(std::holds_alternative<ConditionalNode>(v));
}
