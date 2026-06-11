#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/start_handler.hpp>
#include <attractor/types.hpp>

using namespace attractor;

namespace {

Node make_start_node()
{
    Node n;
    n.id = NodeId{"start"};
    n.shape = NodeShape::mdiamond;
    return n;
}

}  // namespace

SNITCH_TEST_CASE("[start_handler] execute returns Outcome{SUCCESS} -- 2.2-U-001")
{
    StartHandler h;
    Context ctx;
    Graph g;
    LogsRoot lr{"./logs"};

    auto outcome = h.execute(make_start_node(), ctx, g, lr);

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.context_updates.is_object());
    SNITCH_CHECK(outcome.context_updates.empty());
    SNITCH_CHECK(type_safe::get(outcome.preferred_label).empty());
    SNITCH_CHECK(outcome.suggested_next_ids.empty());
}

SNITCH_TEST_CASE("[start_handler] execute via base pointer returns SUCCESS")
{
    StartHandler impl;
    const Handler* h = &impl;
    Context ctx;
    Graph g;
    LogsRoot lr{"./logs"};

    auto outcome = h->execute(make_start_node(), ctx, g, lr);

    SNITCH_CHECK(outcome.status == StageStatus::success);
}
