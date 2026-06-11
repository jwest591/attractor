#include "attractor_test_support.hpp"

#include <attractor/checkpoint.hpp>
#include <attractor/context.hpp>
#include <attractor/engine.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/handlers/exit_handler.hpp>
#include <attractor/handlers/start_handler.hpp>
#include <attractor/types.hpp>
#include <memory>
#include <stdexcept>
#include <string>

using namespace attractor;
using namespace attractor::test;

// -- local test helpers -------------------------------------------------------

namespace {

class ThrowingHandler final : public Handler {
  public:
    [[nodiscard]] auto execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                               const LogsRoot& /*logs_root*/) const -> Outcome override
    {
        throw std::runtime_error("intentional test exception");
    }
};

class RecordingHandler final : public Handler {
  public:
    mutable int call_count{0};

    [[nodiscard]] auto execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                               const LogsRoot& /*logs_root*/) const -> Outcome override
    {
        ++call_count;
        return Outcome{};
    }
};

}  // namespace

// -- AC1: linear pipeline -----------------------------------------------------

SNITCH_TEST_CASE("[engine] 3-node linear pipeline returns SUCCESS -- 2.3-I-001")
{
    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] node_a [shape=box] done [shape=Msquare]
                  start -> node_a -> done }
    )");
    TempLogsDir logs;
    Engine engine;

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
}

SNITCH_TEST_CASE("[engine] 3-node pipeline dispatches intermediate handler exactly once")
{
    auto recording = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw = recording.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"codergen"}, std::move(recording));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] node_a [shape=box] done [shape=Msquare]
                  start -> node_a -> done }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw->call_count == 1);
}

// -- AC2: terminal + goal gate ------------------------------------------------

SNITCH_TEST_CASE("[engine] terminal node stops execution and returns SUCCESS")
{
    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] done [shape=Msquare]
                  start -> done }
    )");
    TempLogsDir logs;
    Engine engine;

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
}

SNITCH_TEST_CASE("[engine] goal gate satisfied: pipeline returns SUCCESS")
{
    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] work [shape=box, goal_gate=true]
                  done [shape=Msquare]
                  start -> work -> done }
    )");
    TempLogsDir logs;
    Engine engine;

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
}

SNITCH_TEST_CASE("[engine] goal gate unsatisfied with no retry_target returns Outcome{fail}")
{
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"gate"}, std::make_unique<ThrowingHandler>());
    reg.set_default_handler(std::make_unique<StartHandler>());

    // gate has goal_gate=true and its handler throws; engine stores Outcome{fail}.
    // At the terminal node, the unsatisfied goal gate with no retry_target must fail
    // the pipeline.
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

// -- AC3: exception boundary --------------------------------------------------

SNITCH_TEST_CASE("[engine] handler throws -> Outcome{fail} without propagation -- 2.3-I-002")
{
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"thrower"}, std::make_unique<ThrowingHandler>());
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond]
                  t [type="thrower", shape=box]
                  done [shape=Msquare]
                  start -> t -> done }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(!outcome.failure_reason.empty());
}

// -- AC4: loop_restart ---------------------------------------------------------

SNITCH_TEST_CASE("[engine] loop_restart=true relaunches at edge target -- 2.3-I-003")
{
    auto recording_a = std::make_unique<RecordingHandler>();
    auto recording_b = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_a = recording_a.get();
    const RecordingHandler* raw_b = recording_b.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"node_a"}, std::move(recording_a));
    reg.register_handler(HandlerTypeName{"node_b"}, std::move(recording_b));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start  [shape=Mdiamond]
            node_a [type="node_a", shape=box]
            node_b [type="node_b", shape=box]
            done   [shape=Msquare]
            start -> node_a
            node_a -> node_b [loop_restart=true]
            node_b -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(raw_a->call_count == 1);
    SNITCH_CHECK(raw_b->call_count == 1);
}

SNITCH_TEST_CASE("[engine] loop_restart carries no goal_gate state from prior run")
{
    // Run 1: gate (goal_gate=true) throws -> Outcome{fail}, then loop_restart to to_done.
    // Run 2 (fresh): to_done succeeds, no goal gates -> pipeline succeeds.
    // Verifies AC4: "prior node outcomes not carried into the new run."
    auto rec_to_done = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_to_done = rec_to_done.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"gate"}, std::make_unique<ThrowingHandler>());
    reg.register_handler(HandlerTypeName{"to_done"}, std::move(rec_to_done));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start   [shape=Mdiamond]
            gate    [type="gate",    shape=box, goal_gate=true]
            to_done [type="to_done", shape=box]
            done    [shape=Msquare]
            start -> gate
            gate  -> to_done [loop_restart=true]
            to_done -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(raw_to_done->call_count == 1);
}

// -- edge selection -----------------------------------------------------------

SNITCH_TEST_CASE("[engine] no outgoing edges from non-terminal node returns SUCCESS")
{
    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] orphan_end [shape=box]
                  start -> orphan_end }
    )");
    TempLogsDir logs;
    Engine engine;

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
}

SNITCH_TEST_CASE("[engine] no start node returns Outcome{fail}")
{
    auto graph = parse_ok(R"(
        digraph { node_a [shape=box] done [shape=Msquare]
                  node_a -> done }
    )");
    TempLogsDir logs;
    Engine engine;

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(!outcome.failure_reason.empty());
}

SNITCH_TEST_CASE("[engine] weight-based edge selection: higher weight wins")
{
    auto rec_high = std::make_unique<RecordingHandler>();
    auto rec_low = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_high = rec_high.get();
    const RecordingHandler* raw_low = rec_low.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"high_node"}, std::move(rec_high));
    reg.register_handler(HandlerTypeName{"low_node"}, std::move(rec_low));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start     [shape=Mdiamond]
            high_node [type="high_node", shape=box]
            low_node  [type="low_node",  shape=box]
            done      [shape=Msquare]
            start -> high_node [weight=10]
            start -> low_node  [weight=1]
            high_node -> done
            low_node  -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_high->call_count == 1);
    SNITCH_CHECK(raw_low->call_count == 0);
}

SNITCH_TEST_CASE("[engine] lexical tiebreak: alpha before beta when weights equal")
{
    auto rec_alpha = std::make_unique<RecordingHandler>();
    auto rec_beta = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_alpha = rec_alpha.get();
    const RecordingHandler* raw_beta = rec_beta.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"alpha_node"}, std::move(rec_alpha));
    reg.register_handler(HandlerTypeName{"beta_node"}, std::move(rec_beta));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start      [shape=Mdiamond]
            alpha_node [type="alpha_node", shape=box]
            beta_node  [type="beta_node",  shape=box]
            done       [shape=Msquare]
            start -> beta_node  [weight=0]
            start -> alpha_node [weight=0]
            alpha_node -> done
            beta_node  -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_alpha->call_count == 1);
    SNITCH_CHECK(raw_beta->call_count == 0);
}

SNITCH_TEST_CASE("[engine] resume from checkpoint skips completed nodes -- 2.6-E-001")
{
    auto recording_a = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_a = recording_a.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"codergen"}, std::move(recording_a));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] node_a [shape=box] done [shape=Msquare]
                  start -> node_a -> done }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    CheckpointData cp;
    cp.current_node = NodeId{"done"};
    cp.completed_nodes = {NodeId{"start"}, NodeId{"node_a"}};
    cp.context = nlohmann::json{{"outcome", "success"}};
    SNITCH_REQUIRE(save_checkpoint(logs.logs_root(), cp).has_value());

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root(), .resume = true});

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(raw_a->call_count == 0);
}
