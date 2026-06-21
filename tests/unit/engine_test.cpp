#include "attractor_test_support.hpp"

#include <attractor/checkpoint.hpp>
#include <attractor/context.hpp>
#include <attractor/engine.hpp>
#include <attractor/events.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/handlers/exit_handler.hpp>
#include <attractor/handlers/start_handler.hpp>
#include <attractor/types.hpp>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_safe/strong_typedef.hpp>
#include <variant>
#include <vector>

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

// -- Story 2.9: Engine(EventObserver) constructor ------------------------------

SNITCH_TEST_CASE("[engine] Engine(EventObserver) calls observer for non-terminal nodes -- 2.9-U-001")
{
    // Red phase: fails to compile until Engine::Engine(EventObserver) is declared in engine.hpp.
    // Observer receives StageStarted + StageCompleted for each non-terminal node.
    // Terminal node (Msquare) emits no events.
    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] node_a [shape=box] done [shape=Msquare]
                  start -> node_a -> done }
    )");
    TempLogsDir logs;

    std::vector<std::string> observed;
    Engine engine{[&observed](const Event& ev) {
        std::visit(
            [&observed](auto&& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, StageStarted>) {
                    observed.push_back("started:" + type_safe::get(e.id));
                }
                else if constexpr (std::is_same_v<T, StageCompleted>) {
                    observed.push_back("completed:" + type_safe::get(e.id));
                }
            },
            ev);
    }};

    const auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_REQUIRE(observed.size() == 4);
    SNITCH_CHECK(observed[0] == "started:start");
    SNITCH_CHECK(observed[1] == "completed:start");
    SNITCH_CHECK(observed[2] == "started:node_a");
    SNITCH_CHECK(observed[3] == "completed:node_a");
}

// -- resolve_fidelity / resolve_thread_key (Story 4.4) --

SNITCH_TEST_CASE("[engine] resolve_fidelity defaults to compact when nothing set -- 4.4-U-001")
{
    Graph g;
    Node n;
    n.id = NodeId{"work"};
    SNITCH_CHECK(resolve_fidelity(n, nullptr, g) == FidelityMode::compact);
}

SNITCH_TEST_CASE("[engine] resolve_fidelity: edge fidelity beats node fidelity -- 4.4-U-002")
{
    Graph g;
    Node n;
    n.id       = NodeId{"work"};
    n.fidelity = FidelityMode::truncate;
    Edge e;
    e.from     = NodeId{"prev"};
    e.to       = NodeId{"work"};
    e.fidelity = FidelityMode::full;
    SNITCH_CHECK(resolve_fidelity(n, &e, g) == FidelityMode::full);
}

SNITCH_TEST_CASE("[engine] resolve_fidelity: graph default_fidelity used when node has none -- 4.4-U-003")
{
    Graph g;
    g.default_fidelity = FidelityMode::summary_high;
    Node n;
    n.id = NodeId{"work"};
    SNITCH_CHECK(resolve_fidelity(n, nullptr, g) == FidelityMode::summary_high);
}

SNITCH_TEST_CASE("[engine] resolve_thread_key: shared thread_id maps to same session key -- 4.4-U-004")
{
    Graph g;
    Node n1;
    n1.id        = NodeId{"plan"};
    n1.thread_id = ThreadId{"loop-a"};
    Node n2;
    n2.id        = NodeId{"implement"};
    n2.thread_id = ThreadId{"loop-a"};
    const auto key1 = resolve_thread_key(n1, nullptr, g);
    const auto key2 = resolve_thread_key(n2, nullptr, g);
    SNITCH_CHECK(type_safe::get(key1) == "loop-a");
    SNITCH_CHECK(type_safe::get(key2) == "loop-a");
    SNITCH_CHECK(type_safe::get(key1) == type_safe::get(key2));
}

SNITCH_TEST_CASE("[engine] resolve_fidelity: edge present but no fidelity falls through to node -- 4.4-U-006")
{
    Graph g;
    Node n;
    n.id       = NodeId{"work"};
    n.fidelity = FidelityMode::truncate;
    Edge e;
    e.from = NodeId{"prev"};
    e.to   = NodeId{"work"};
    // edge has no fidelity -> falls through to node fidelity
    SNITCH_CHECK(resolve_fidelity(n, &e, g) == FidelityMode::truncate);
}

SNITCH_TEST_CASE("[engine] resolve_thread_key: edge thread_id used when node has none -- 4.4-U-007")
{
    Graph g;
    Node n;
    n.id        = NodeId{"work"};
    Edge e;
    e.from      = NodeId{"prev"};
    e.to        = NodeId{"work"};
    e.thread_id = ThreadId{"edge-thread"};
    const auto key = resolve_thread_key(n, &e, g);
    SNITCH_CHECK(type_safe::get(key) == "edge-thread");
}

// -- Engine(backend, observer) wiring (Story 5.1) --

SNITCH_TEST_CASE("[engine] Engine(backend, observer) uses injected backend not NoOpBackend -- 5.1-U-001")
{
    struct SpyBackend final : public CodergenBackend {
        mutable bool called{false};
        [[nodiscard]] auto run(const Node& /*node*/, const PromptText& /*prompt*/,
                               Context& /*ctx*/) const -> std::expected<LlmResponse, Outcome> override
        {
            called = true;
            return LlmResponse{"spy-response"};
        }
    };

    auto spy = std::make_unique<SpyBackend>();
    SpyBackend* spy_ptr = spy.get();
    TempLogsDir logs;

    auto result = parse_graph(R"(
        digraph g {
            start [shape=Mdiamond]
            work  [shape=box, prompt="Do work"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    SNITCH_REQUIRE(result.has_value());

    Engine engine{std::move(spy), nullptr};
    const auto outcome = engine.run(*result, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(spy_ptr->called);
}

// -- AC #1: loop_restart depth guard (Story 7.4) ------------------------------

SNITCH_TEST_CASE("[engine] loop_restart depth limit returns FAIL when exceeded -- 7.4-U-001")
{
    // loop_n has no explicit type; the default Engine uses StartHandler as fallback,
    // which returns success. This lets the engine follow the loop_restart edge on
    // every iteration until the depth guard fires.
    auto graph = parse_ok(R"(
        digraph {
            start  [shape=Mdiamond]
            loop_n [shape=box]
            start  -> loop_n
            loop_n -> loop_n [loop_restart=true]
        }
    )");
    TempLogsDir logs;
    Engine engine;

    const auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root(), .max_loop_depth = 5});

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(type_safe::get(outcome.failure_reason).find("loop_restart depth limit") != std::string::npos);
}

// -- AC #2: multi-gate terminal pass (Story 7.4) ------------------------------

SNITCH_TEST_CASE("[engine] multi-gate terminal: gate without retry_target fails pipeline immediately -- 7.4-U-002")
{
    // alpha_gate (alphabetically first) always fails but has a retry_target.
    // beta_gate always fails and has NO retry_target.
    // Old code would retry alpha_gate (break after first gate with retry_target),
    // never evaluating beta_gate's missing retry_target on the first pass.
    // New code processes all unsatisfied gates: finds beta_gate has no retry_target,
    // fails immediately -- alpha_gate is called exactly once.
    class AlwaysFailHandler final : public Handler {
      public:
        mutable int call_count{0};
        [[nodiscard]] auto execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                                   const LogsRoot& /*logs_root*/) const -> Outcome override
        {
            ++call_count;
            return Outcome::fail(DiagnosticMessage{"always fails"});
        }
    };

    auto alpha_fail = std::make_unique<AlwaysFailHandler>();
    const AlwaysFailHandler* raw_alpha_fail = alpha_fail.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"alpha_gate"}, std::move(alpha_fail));
    reg.register_handler(HandlerTypeName{"beta_gate"}, std::make_unique<AlwaysFailHandler>());
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start      [shape=Mdiamond]
            alpha_gate [type="alpha_gate", shape=box, goal_gate=true, retry_target=start]
            beta_gate  [type="beta_gate",  shape=box, goal_gate=true]
            done       [shape=Msquare]
            start -> alpha_gate -> beta_gate -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    const auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    // Pipeline must fail because beta_gate has no retry_target.
    SNITCH_CHECK(outcome.status == StageStatus::fail);
    // With new multi-gate pass, all gates are evaluated before deciding to retry.
    // beta_gate has no retry_target so pipeline fails on the first terminal pass:
    // alpha_gate should be called exactly 1 time.
    SNITCH_CHECK(raw_alpha_fail->call_count == 1);
}

// -- AC #4: constructor handler parity (Story 7.4) ----------------------------

SNITCH_TEST_CASE("[engine] Engine(backend) constructor registers all default handlers -- 7.4-U-003")
{
    // Verifies that Engine(unique_ptr<CodergenBackend>) calls register_default_handlers
    // by running a codergen node through the backend-injection constructor.
    struct RecordingBackend final : public CodergenBackend {
        mutable bool called{false};
        [[nodiscard]] auto run(const Node& /*node*/, const PromptText& /*prompt*/,
                               Context& /*ctx*/) const -> std::expected<LlmResponse, Outcome> override
        {
            called = true;
            return LlmResponse{"ok"};
        }
    };

    auto backend = std::make_unique<RecordingBackend>();
    RecordingBackend* raw_backend = backend.get();
    TempLogsDir logs;

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            work  [shape=box, prompt="go"]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");

    Engine engine_with_backend{std::move(backend)};
    const auto outcome = engine_with_backend.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(raw_backend->called);
}
