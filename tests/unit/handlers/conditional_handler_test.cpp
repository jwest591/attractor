#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/conditional_handler.hpp>
#include <attractor/types.hpp>

using namespace attractor;

namespace {

Node make_conditional_node(std::string id)
{
    Node n;
    n.id = NodeId{std::move(id)};
    n.shape = NodeShape::diamond;
    return n;
}

}  // namespace

SNITCH_TEST_CASE("[conditional_handler] execute returns SUCCESS with notes -- 2.2-U-004")
{
    ConditionalHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    auto outcome = h.execute(make_conditional_node("check_quality"), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(type_safe::get(outcome.notes) == "Conditional node evaluated: check_quality");
}

SNITCH_TEST_CASE("[conditional_handler] notes contain the node id")
{
    ConditionalHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    auto outcome = h.execute(make_conditional_node("route_decision"), ctx, g, rc);

    SNITCH_CHECK(type_safe::get(outcome.notes) == "Conditional node evaluated: route_decision");
}

SNITCH_TEST_CASE("[conditional_handler] no context updates and no preferred label")
{
    ConditionalHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    auto outcome = h.execute(make_conditional_node("x"), ctx, g, rc);

    SNITCH_CHECK(outcome.context_updates.empty());
    SNITCH_CHECK(type_safe::get(outcome.preferred_label).empty());
    SNITCH_CHECK(outcome.suggested_next_ids.empty());
}
