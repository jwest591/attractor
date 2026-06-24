#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/manager_loop_handler.hpp>
#include <attractor/types.hpp>
#include <string>

using namespace attractor;

namespace {

ManagerNode make_manager_node(std::string id, int max_cycles, std::string stop_condition = "")
{
    ManagerNode n;
    n.id = NodeId{std::move(id)};
    n.shape = NodeShape::house;
    n.manager_max_cycles = max_cycles;
    n.manager_stop_condition = ConditionExpr{std::move(stop_condition)};
    return n;
}

}  // namespace

SNITCH_TEST_CASE("[manager_loop_handler] max_cycles=3 with never-satisfied condition returns FAIL -- 2.8-U-004")
{
    ManagerLoopHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    auto outcome = h.execute(make_manager_node("loop", 3, "context.nope=yes"), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(type_safe::get(outcome.failure_reason) == "Max cycles exceeded");
}

SNITCH_TEST_CASE("[manager_loop_handler] stop condition met on cycle 2 returns SUCCESS -- 2.8-U-005")
{
    ManagerLoopHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    auto outcome = h.execute(make_manager_node("loop", 5, "context.cycle=2"), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::success);
    const int final_cycle = std::stoi(ctx.get(ContextKey{"cycle"}).get<std::string>());
    SNITCH_CHECK(final_cycle == 2);
    SNITCH_CHECK(final_cycle < 5);
}

SNITCH_TEST_CASE("[manager_loop_handler] empty stop condition exhausts max_cycles")
{
    ManagerLoopHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    auto outcome = h.execute(make_manager_node("loop", 2, ""), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(type_safe::get(outcome.failure_reason) == "Max cycles exceeded");
}
