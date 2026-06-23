#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/types.hpp>
#include <memory>
#include <string>

using namespace attractor;

// -- helpers ------------------------------------------------------------------

namespace {

struct StubHandler final : public Handler {
    StageStatus return_status{StageStatus::success};
    mutable int call_count{0};

    [[nodiscard]] auto execute(const Node&, Context&, const Graph&, const RunConfig&) const -> Outcome override
    {
        ++call_count;
        return Outcome{.status = return_status};
    }
};

Node make_node(std::string id, NodeShape shape = NodeShape::box, std::string handler_type = "")
{
    Node n;
    n.id = NodeId{std::move(id)};
    n.shape = shape;
    n.node_type = HandlerTypeName{std::move(handler_type)};
    return n;
}

}  // namespace

// -- AC2: Outcome construction ------------------------------------------------

SNITCH_TEST_CASE("[handler_registry] Outcome default-constructed has success status and empty fields")
{
    Outcome o;
    SNITCH_CHECK(o.status == StageStatus::success);
    SNITCH_CHECK(type_safe::get(o.preferred_label).empty());
    SNITCH_CHECK(o.suggested_next_ids.empty());
    SNITCH_CHECK(o.context_updates.is_object());
    SNITCH_CHECK(o.context_updates.empty());
    SNITCH_CHECK(type_safe::get(o.notes).empty());
    SNITCH_CHECK(type_safe::get(o.failure_reason).empty());
}

SNITCH_TEST_CASE("[handler_registry] Outcome designated initialiser sets named fields, others stay defaulted")
{
    Outcome o{.status = StageStatus::fail, .failure_reason = DiagnosticMessage{"boom"}};
    SNITCH_CHECK(o.status == StageStatus::fail);
    SNITCH_CHECK(type_safe::get(o.failure_reason) == "boom");
    SNITCH_CHECK(type_safe::get(o.preferred_label).empty());
    SNITCH_CHECK(type_safe::get(o.notes).empty());
}

SNITCH_TEST_CASE("[handler_registry] Outcome::fail factory sets status=fail and reason")
{
    auto o = Outcome::fail(DiagnosticMessage{"something broke"});
    SNITCH_CHECK(o.status == StageStatus::fail);
    SNITCH_CHECK(type_safe::get(o.failure_reason) == "something broke");
    SNITCH_CHECK(type_safe::get(o.preferred_label).empty());
}

SNITCH_TEST_CASE("[handler_registry] Outcome context_updates accepts JSON values")
{
    Outcome o;
    o.context_updates["last_stage"] = "my_node";
    o.context_updates["score"] = 42;
    SNITCH_CHECK(o.context_updates["last_stage"] == "my_node");
    SNITCH_CHECK(o.context_updates["score"] == 42);
}

SNITCH_TEST_CASE("[handler_registry] Outcome with all fields set compiles cleanly")
{
    Outcome o{
        .status = StageStatus::partial_success,
        .preferred_label = EdgeLabel{"Fix"},
        .suggested_next_ids = {NodeId{"next_a"}, NodeId{"next_b"}},
        .notes = HandlerNote{"partial done"},
        .failure_reason = DiagnosticMessage{""},
    };
    o.context_updates["x"] = true;
    SNITCH_CHECK(o.status == StageStatus::partial_success);
    SNITCH_CHECK(type_safe::get(o.preferred_label) == "Fix");
    SNITCH_CHECK(o.suggested_next_ids.size() == 2U);
}

// -- AC1: Handler interface ---------------------------------------------------

SNITCH_TEST_CASE("[handler_registry] Handler::execute called on concrete impl returns Outcome")
{
    StubHandler h;
    h.return_status = StageStatus::success;
    Context ctx;
    Graph g;
    Node n = make_node("work");

    auto outcome = h.execute(n, ctx, g, RunConfig{.logs_root = LogsRoot{"./logs"}});
    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(h.call_count == 1);
}

SNITCH_TEST_CASE("[handler_registry] Handler called via base reference returns correct Outcome")
{
    StubHandler impl;
    impl.return_status = StageStatus::retry;
    const Handler& h = impl;

    Context ctx;
    Graph g;
    Node n = make_node("work");

    auto outcome = h.execute(n, ctx, g, RunConfig{.logs_root = LogsRoot{"./logs"}});
    SNITCH_CHECK(outcome.status == StageStatus::retry);
    SNITCH_CHECK(impl.call_count == 1);
}

// -- AC3/AC4: HandlerRegistry -------------------------------------------------

SNITCH_TEST_CASE("[handler_registry] register_handler and resolve by explicit type - 2.1-U-001")
{
    HandlerRegistry reg;
    auto h1 = std::make_unique<StubHandler>();
    auto default_h = std::make_unique<StubHandler>();
    auto* h1_ptr = h1.get();
    auto* default_h_ptr = default_h.get();
    reg.set_default_handler(std::move(default_h));
    reg.register_handler(HandlerTypeName{"my_handler"}, std::move(h1));

    Node n = make_node("x", NodeShape::box, "my_handler");
    SNITCH_CHECK(&reg.resolve(n) == h1_ptr);
    SNITCH_CHECK(&reg.resolve(n) != default_h_ptr);
}

SNITCH_TEST_CASE("[handler_registry] second registration replaces first - 2.1-U-002")
{
    HandlerRegistry reg;
    auto h1 = std::make_unique<StubHandler>();
    auto h2 = std::make_unique<StubHandler>();
    auto default_h = std::make_unique<StubHandler>();
    auto* h1_ptr = h1.get();
    auto* h2_ptr = h2.get();
    reg.set_default_handler(std::move(default_h));

    reg.register_handler(HandlerTypeName{"my_type"}, std::move(h1));
    reg.register_handler(HandlerTypeName{"my_type"}, std::move(h2));

    Node n = make_node("x", NodeShape::box, "my_type");
    SNITCH_CHECK(&reg.resolve(n) == h2_ptr);
    SNITCH_CHECK(&reg.resolve(n) != h1_ptr);
}

SNITCH_TEST_CASE("[handler_registry] unregistered explicit type falls back to shape - 2.1-U-003")
{
    HandlerRegistry reg;
    auto codergen = std::make_unique<StubHandler>();
    auto default_h = std::make_unique<StubHandler>();
    auto* codergen_ptr = codergen.get();
    reg.set_default_handler(std::move(default_h));
    reg.register_handler(HandlerTypeName{"codergen"}, std::move(codergen));

    // "unknown_type" not registered - falls back to shape "box" -> "codergen"
    Node n = make_node("x", NodeShape::box, "unknown_type");
    SNITCH_CHECK(&reg.resolve(n) == codergen_ptr);
}

SNITCH_TEST_CASE("[handler_registry] all 9 shapes resolve to correct registered handlers")
{
    HandlerRegistry reg;
    auto default_h = std::make_unique<StubHandler>();
    reg.set_default_handler(std::move(default_h));

    struct ShapeCase {
        NodeShape shape;
        const char* type;
    };

    const ShapeCase k_cases[] = {
        {NodeShape::mdiamond,       "start"             },
        {NodeShape::msquare,        "exit"              },
        {NodeShape::box,            "codergen"          },
        {NodeShape::hexagon,        "wait.human"        },
        {NodeShape::diamond,        "conditional"       },
        {NodeShape::component,      "parallel"          },
        {NodeShape::triple_octagon, "parallel.fan_in"   },
        {NodeShape::parallelogram,  "tool"              },
        {NodeShape::house,          "stack.manager_loop"},
    };

    StubHandler* k_expected[std::size(k_cases)] = {};
    for (std::size_t i = 0; i < std::size(k_cases); ++i) {
        auto h = std::make_unique<StubHandler>();
        k_expected[i] = h.get();
        reg.register_handler(HandlerTypeName{k_cases[i].type}, std::move(h));
    }

    for (std::size_t i = 0; i < std::size(k_cases); ++i) {
        Node n = make_node("x", k_cases[i].shape, "");  // no explicit type -> shape resolution
        SNITCH_CHECK(&reg.resolve(n) == k_expected[i]);
    }
}

SNITCH_TEST_CASE("[handler_registry] unregistered type and unregistered shape handler returns default")
{
    HandlerRegistry reg;
    auto default_h = std::make_unique<StubHandler>();
    auto* default_h_ptr = default_h.get();
    reg.set_default_handler(std::move(default_h));

    // "unknown_type" not registered; "box" maps to "codergen" which is also not registered -> default
    Node n = make_node("x", NodeShape::box, "unknown_type");
    SNITCH_CHECK(&reg.resolve(n) == default_h_ptr);
}

SNITCH_TEST_CASE("[handler_registry] node with no explicit type uses shape resolution directly")
{
    HandlerRegistry reg;
    auto start_h = std::make_unique<StubHandler>();
    auto default_h = std::make_unique<StubHandler>();
    auto* start_h_ptr = start_h.get();
    reg.set_default_handler(std::move(default_h));
    reg.register_handler(HandlerTypeName{"start"}, std::move(start_h));

    Node n = make_node("begin", NodeShape::mdiamond, "");  // no explicit type
    SNITCH_CHECK(&reg.resolve(n) == start_h_ptr);
}
