#include "attractor_test_support.hpp"

#include <attractor/backends/noop_backend.hpp>
#include <attractor/context.hpp>
#include <attractor/engine.hpp>
#include <attractor/events.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/handlers/codergen_handler.hpp>
#include <attractor/handlers/exit_handler.hpp>
#include <attractor/handlers/start_handler.hpp>
#include <attractor/types.hpp>
#include <filesystem>
#include <memory>
#include <variant>
#include <vector>

using namespace attractor;
using namespace attractor::test;

namespace {

class NoOpHandler final : public Handler {
  public:
    [[nodiscard]] auto execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                               const RunConfig& /*run_config*/) const -> Outcome override
    {
        return Outcome{};
    }
};

HandlerRegistry make_basic_registry()
{
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.set_default_handler(std::make_unique<NoOpHandler>());
    return reg;
}

}  // namespace

SNITCH_TEST_CASE("[events] StageStarted emitted with correct NodeId and index=0 for first node -- 2.7-EV-001")
{
    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] node_a [shape=box] done [shape=Msquare]
                  start -> node_a -> done }
    )");

    std::vector<Event> captured;
    EventObserver observer = [&](const Event& e) { captured.push_back(e); };

    Engine engine{make_basic_registry(), std::move(observer)};
    TempLogsDir logs;
    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_REQUIRE(outcome.status == StageStatus::success);

    // start[index=0], node_a[index=1]; terminal done emits nothing
    // Each non-terminal node emits StageStarted then StageCompleted
    SNITCH_REQUIRE(captured.size() == 4);

    SNITCH_REQUIRE(std::holds_alternative<StageStarted>(captured[0]));
    const auto& ev0 = std::get<StageStarted>(captured[0]);
    SNITCH_CHECK(ev0.id == NodeId{"start"});
    SNITCH_CHECK(ev0.index == 0);

    SNITCH_REQUIRE(std::holds_alternative<StageCompleted>(captured[1]));
    const auto& ev1 = std::get<StageCompleted>(captured[1]);
    SNITCH_CHECK(ev1.id == NodeId{"start"});
    SNITCH_CHECK(ev1.index == 0);
}

SNITCH_TEST_CASE("[events] StageStarted and StageCompleted indices are sequential -- 2.7-EV-002")
{
    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] a [shape=box] b [shape=box] done [shape=Msquare]
                  start -> a -> b -> done }
    )");

    std::vector<int> started_indices;
    std::vector<int> completed_indices;

    EventObserver observer = [&](const Event& e) {
        if (std::holds_alternative<StageStarted>(e)) {
            started_indices.push_back(std::get<StageStarted>(e).index);
        }
        else if (std::holds_alternative<StageCompleted>(e)) {
            completed_indices.push_back(std::get<StageCompleted>(e).index);
        }
    };

    Engine engine{make_basic_registry(), std::move(observer)};
    TempLogsDir logs;
    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_REQUIRE(outcome.status == StageStatus::success);
    // start[0], a[1], b[2]; terminal done emits nothing
    SNITCH_REQUIRE(started_indices.size() == 3);
    SNITCH_CHECK(started_indices[0] == 0);
    SNITCH_CHECK(started_indices[1] == 1);
    SNITCH_CHECK(started_indices[2] == 2);
    SNITCH_CHECK(completed_indices == started_indices);
}

SNITCH_TEST_CASE("[events] null EventObserver (default Engine) does not crash -- 2.7-EV-003")
{
    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] done [shape=Msquare] start -> done }
    )");

    Engine engine{};  // default constructor -- m_on_event is null std::function
    TempLogsDir logs;
    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
}

SNITCH_TEST_CASE("[events] StageStarted NodeIds match executed non-terminal nodes in order -- 2.7-EV-004")
{
    auto graph = parse_ok(R"(
        digraph { start [shape=Mdiamond] process [shape=box] done [shape=Msquare]
                  start -> process -> done }
    )");

    std::vector<NodeId> started_ids;
    EventObserver observer = [&](const Event& e) {
        if (std::holds_alternative<StageStarted>(e)) {
            started_ids.push_back(std::get<StageStarted>(e).id);
        }
    };

    Engine engine{make_basic_registry(), std::move(observer)};
    TempLogsDir logs;
    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_REQUIRE(started_ids.size() == 2);
    SNITCH_CHECK(started_ids[0] == NodeId{"start"});
    SNITCH_CHECK(started_ids[1] == NodeId{"process"});
}

SNITCH_TEST_CASE("[events] codergen node writes prompt.md response.md status.json (DoD S11.7-6) -- 2.7-EV-005")
{
    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            gen   [shape=box, prompt="test prompt"]
            done  [shape=Msquare]
            start -> gen -> done
        }
    )");

    HandlerRegistry reg;
    NoOpBackend noop;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"codergen"}, std::make_unique<CodergenHandler>(&noop));

    TempLogsDir logs;
    Engine engine{std::move(reg)};
    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_REQUIRE(outcome.status == StageStatus::success);

    // DoD S11.7-6: per-node artifacts written to {logs_root}/{node_id}/
    SNITCH_CHECK(std::filesystem::exists(logs.path() / "gen" / "prompt.md"));
    SNITCH_CHECK(std::filesystem::exists(logs.path() / "gen" / "response.md"));
    SNITCH_CHECK(std::filesystem::exists(logs.path() / "gen" / "status.json"));
}
