#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/engine.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/handlers/exit_handler.hpp>
#include <attractor/handlers/start_handler.hpp>
#include <attractor/types.hpp>
#include <memory>

using namespace attractor;
using namespace attractor::test;

namespace {

// Returns a fixed outcome regardless of node state.
class FixedOutcomeHandler final : public Handler {
  public:
    explicit FixedOutcomeHandler(StageStatus s) : m_status{s} {}

    [[nodiscard]] auto execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                               const LogsRoot& /*logs_root*/) const -> Outcome override
    {
        return Outcome{.status = m_status};
    }

  private:
    StageStatus m_status;
};

// Fails on the first call, succeeds on all subsequent calls.
class FailOnceThenSucceedHandler final : public Handler {
  public:
    mutable int call_count{0};

    [[nodiscard]] auto execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                               const LogsRoot& /*logs_root*/) const -> Outcome override
    {
        ++call_count;
        if (call_count == 1) {
            return Outcome::fail(DiagnosticMessage{"first attempt fails"});
        }
        return Outcome{};
    }
};

}  // namespace

// -- DoD 11.4-1: node-level retry_target --------------------------------------

SNITCH_TEST_CASE("[goal_gate] goal_gate=true FAIL routes to node-level retry_target -- 2.5-U-001")
{
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"gate"}, std::make_unique<FailOnceThenSucceedHandler>());
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            work  [shape=box]
            gate  [type="gate", shape=box, goal_gate=true, retry_target=work]
            done  [shape=Msquare]
            start -> work -> gate -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
}

// -- DoD 11.4-2: graph-level retry_target -------------------------------------

SNITCH_TEST_CASE("[goal_gate] goal_gate=true FAIL routes to graph-level retry_target -- 2.5-U-002")
{
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"gate"}, std::make_unique<FailOnceThenSucceedHandler>());
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            graph [retry_target="work"]
            start [shape=Mdiamond]
            work  [shape=box]
            gate  [type="gate", shape=box, goal_gate=true]
            done  [shape=Msquare]
            start -> work -> gate -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
}

// -- DoD 11.4-3: no retry_target anywhere -> FAIL + reason --------------------

SNITCH_TEST_CASE("[goal_gate] goal_gate=true FAIL with no retry_target returns FAIL -- 2.5-U-003")
{
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"gate"},
                         std::make_unique<FixedOutcomeHandler>(StageStatus::fail));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            gate  [type="gate", shape=box, goal_gate=true]
            done  [shape=Msquare]
            start -> gate -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(!outcome.failure_reason.empty());
}

// -- DoD 11.4-4: SUCCESS exits normally ---------------------------------------

SNITCH_TEST_CASE("[goal_gate] goal_gate=true SUCCESS exits pipeline normally -- 2.5-U-004")
{
    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            gate  [shape=box, goal_gate=true]
            done  [shape=Msquare]
            start -> gate -> done
        }
    )");
    TempLogsDir logs;
    Engine engine;

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
}

// -- DoD 11.4-5: PARTIAL_SUCCESS treated as satisfied ------------------------

SNITCH_TEST_CASE("[goal_gate] goal_gate=true PARTIAL_SUCCESS treated as satisfied -- 2.5-U-005")
{
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"gate"},
                         std::make_unique<FixedOutcomeHandler>(StageStatus::partial_success));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            gate  [type="gate", shape=box, goal_gate=true]
            done  [shape=Msquare]
            start -> gate -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    // partial_success satisfies the goal gate: pipeline exits normally.
    SNITCH_CHECK(outcome.status == StageStatus::success);
}

// -- DoD 11.4-6: stale outcome cleared; gate re-executed ----------------------

SNITCH_TEST_CASE("[goal_gate] stale outcome cleared: gate re-executed on second pass -- 2.5-U-006")
{
    auto gate_ptr = std::make_unique<FailOnceThenSucceedHandler>();
    const FailOnceThenSucceedHandler* raw_gate = gate_ptr.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"gate"}, std::move(gate_ptr));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            work  [shape=box]
            gate  [type="gate", shape=box, goal_gate=true, retry_target=work]
            done  [shape=Msquare]
            start -> work -> gate -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
    // Gate must have been called twice: once to FAIL (first pass) and once to SUCCEED (retry pass).
    // A call_count of 1 means the stale FAIL outcome was not cleared and the gate was not re-executed.
    SNITCH_CHECK(raw_gate->call_count == 2);
}
