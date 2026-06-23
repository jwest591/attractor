#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/exit_handler.hpp>
#include <attractor/types.hpp>

using namespace attractor;

namespace {

Node make_exit_node()
{
    Node n;
    n.id = NodeId{"exit"};
    n.shape = NodeShape::msquare;
    return n;
}

}  // namespace

SNITCH_TEST_CASE("[exit_handler] execute returns Outcome{SUCCESS} -- 2.2-U-002")
{
    ExitHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    auto outcome = h.execute(make_exit_node(), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.context_updates.is_object());
    SNITCH_CHECK(outcome.context_updates.empty());
    SNITCH_CHECK(type_safe::get(outcome.preferred_label).empty());
    SNITCH_CHECK(outcome.suggested_next_ids.empty());
}

SNITCH_TEST_CASE("[exit_handler] execute via base pointer returns SUCCESS")
{
    ExitHandler impl;
    const Handler* h = &impl;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    auto outcome = h->execute(make_exit_node(), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::success);
}
