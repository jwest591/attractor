#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/engine.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/handlers/exit_handler.hpp>
#include <attractor/handlers/start_handler.hpp>
#include <attractor/types.hpp>
#include <chrono>
#include <memory>
#include <vector>

using namespace attractor;
using namespace attractor::test;

namespace {

// Returns StageStatus::retry n times, then StageStatus::success.
class RetryNTimesHandler final : public Handler {
  public:
    explicit RetryNTimesHandler(int n) : m_retries_left{n} {}

    mutable int call_count{0};

    [[nodiscard]] auto execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                               const LogsRoot& /*logs_root*/) const -> Outcome override
    {
        ++call_count;
        if (m_retries_left > 0) {
            --m_retries_left;
            return Outcome{.status = StageStatus::retry};
        }
        return Outcome{};
    }

  private:
    mutable int m_retries_left;
};

}  // namespace

// -- DoD 11.5-1: max_retries=2 → exactly 3 executions ------------------------

SNITCH_TEST_CASE("[retry] max_retries=2 persistent RETRY executes node exactly 3 times -- 2.5-U-007")
{
    auto handler_ptr = std::make_unique<RetryNTimesHandler>(999);
    const RetryNTimesHandler* raw = handler_ptr.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"work"}, std::move(handler_ptr));
    reg.set_default_handler(std::make_unique<StartHandler>());

    std::vector<std::chrono::duration<double>> recorded_sleeps;
    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            work  [type="work", shape=box, max_retries=2]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{
        .logs_root = logs.logs_root(),
        .retry_policy = {
            .preset = BackoffPreset::none,
            .sleep_fn = [&](std::chrono::duration<double> d) { recorded_sleeps.push_back(d); }
        }
    });

    SNITCH_CHECK(raw->call_count == 3);
    SNITCH_CHECK(outcome.status == StageStatus::fail);
}

// -- DoD 11.5-2: max_retries=0 → 1 execution only ----------------------------

SNITCH_TEST_CASE("[retry] max_retries=0 node returning RETRY executes only once -- 2.5-U-008")
{
    auto handler_ptr = std::make_unique<RetryNTimesHandler>(999);
    const RetryNTimesHandler* raw = handler_ptr.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"work"}, std::move(handler_ptr));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            work  [type="work", shape=box, max_retries=0]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{
        .logs_root = logs.logs_root(),
        .retry_policy = {.sleep_fn = [](std::chrono::duration<double>) {}}
    });

    SNITCH_CHECK(raw->call_count == 1);
    SNITCH_CHECK(outcome.status == StageStatus::fail);
}

// -- DoD 11.5-3: no node max_retries, graph default_max_retries=0 → 1 exec ---

SNITCH_TEST_CASE("[retry] no node max_retries with default_max_retries=0 executes only once -- 2.5-U-009")
{
    auto handler_ptr = std::make_unique<RetryNTimesHandler>(999);
    const RetryNTimesHandler* raw = handler_ptr.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"work"}, std::move(handler_ptr));
    reg.set_default_handler(std::make_unique<StartHandler>());

    // No max_retries on the node; graph explicitly sets default_max_retries=0.
    auto graph = parse_ok(R"(
        digraph {
            graph [default_max_retries=0]
            start [shape=Mdiamond]
            work  [type="work", shape=box]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{
        .logs_root = logs.logs_root(),
        .retry_policy = {.sleep_fn = [](std::chrono::duration<double>) {}}
    });

    SNITCH_CHECK(raw->call_count == 1);
    SNITCH_CHECK(outcome.status == StageStatus::fail);
}

// -- DoD 11.5-4: sleep_fn called for every backoff delay (ASR-1) -------------

SNITCH_TEST_CASE("[retry] sleep_fn injected via RetryPolicy is called for every backoff delay -- 2.5-U-010")
{
    // max_retries=2: 2 retries → sleep_fn called exactly 2 times.
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"work"}, std::make_unique<RetryNTimesHandler>(999));
    reg.set_default_handler(std::make_unique<StartHandler>());

    std::vector<std::chrono::duration<double>> recorded_sleeps;
    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            work  [type="work", shape=box, max_retries=2]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{
        .logs_root = logs.logs_root(),
        .retry_policy = {
            .preset = BackoffPreset::fixed_1s,
            .sleep_fn = [&](std::chrono::duration<double> d) { recorded_sleeps.push_back(d); }
        }
    });

    SNITCH_CHECK(recorded_sleeps.size() == 2u);
}

// -- DoD 11.5-5: allow_partial=true + exhausted → PARTIAL_SUCCESS -------------

SNITCH_TEST_CASE("[retry] allow_partial=true exhausted retries returns PARTIAL_SUCCESS -- 2.5-U-011")
{
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"work"}, std::make_unique<RetryNTimesHandler>(999));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            work  [type="work", shape=box, max_retries=1, allow_partial=true]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{
        .logs_root = logs.logs_root(),
        .retry_policy = {.sleep_fn = [](std::chrono::duration<double>) {}}
    });

    SNITCH_CHECK(outcome.status == StageStatus::partial_success);
}

// -- DoD 11.5-6: allow_partial=false + exhausted → FAIL ----------------------

SNITCH_TEST_CASE("[retry] allow_partial=false exhausted retries returns FAIL -- 2.5-U-012")
{
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"work"}, std::make_unique<RetryNTimesHandler>(999));
    reg.set_default_handler(std::make_unique<StartHandler>());

    // allow_partial defaults to false; max_retries=1 → 1 retry → exhausted.
    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            work  [type="work", shape=box, max_retries=1]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{
        .logs_root = logs.logs_root(),
        .retry_policy = {.sleep_fn = [](std::chrono::duration<double>) {}}
    });

    SNITCH_CHECK(outcome.status == StageStatus::fail);
}

// -- DoD 11.5-7: exponential_jitter_1s attempt-0 delay in [0.75s, 1.25s] ----

SNITCH_TEST_CASE("[retry] exponential_jitter_1s attempt-0 delay in [0.75s, 1.25s] -- 2.5-U-013")
{
    // RetryNTimesHandler(1) retries once → 1 sleep call at attempt index 0.
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"work"}, std::make_unique<RetryNTimesHandler>(1));
    reg.set_default_handler(std::make_unique<StartHandler>());

    std::vector<std::chrono::duration<double>> recorded_sleeps;
    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            work  [type="work", shape=box, max_retries=1]
            done  [shape=Msquare]
            start -> work -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{
        .logs_root = logs.logs_root(),
        .retry_policy = {
            .preset = BackoffPreset::exponential_jitter_1s,
            .sleep_fn = [&](std::chrono::duration<double> d) { recorded_sleeps.push_back(d); }
        }
    });

    // Formula: base = 1s * 2^0 = 1s, jitter +-25% → [0.75s, 1.25s].
    SNITCH_REQUIRE(recorded_sleeps.size() == 1u);
    SNITCH_CHECK(recorded_sleeps[0].count() >= 0.75);
    SNITCH_CHECK(recorded_sleeps[0].count() <= 1.25);
}
