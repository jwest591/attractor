#include "attractor_test_support.hpp"

#include <attractor/engine.hpp>
#include <attractor/events.hpp>
#include <attractor/types.hpp>

#include <filesystem>
#include <string>
#include <type_safe/strong_typedef.hpp>
#include <variant>
#include <vector>

using namespace attractor;
using namespace attractor::test;

SNITCH_TEST_CASE("[integration][smoke] 3-node pipeline runs end-to-end with NoOp backend -- 2.9-I-001")
{
    // start (Mdiamond) -> work (box/codergen) -> done (Msquare)
    // Engine default ctor registers CodergenHandler(NoOpBackend) for shape=box.
    auto graph = parse_ok(R"(
        digraph pipeline {
            start [shape=Mdiamond];
            work  [shape=box, prompt="say hello"];
            done  [shape=Msquare];
            start -> work;
            work  -> done;
        }
    )");

    TempLogsDir logs;

    std::vector<std::string> events_log;
    Engine engine{[&events_log](const Event& ev) {
        std::visit(
            [&events_log](auto&& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, StageStarted>) {
                    events_log.push_back("started:" + type_safe::get(e.id));
                }
                else if constexpr (std::is_same_v<T, StageCompleted>) {
                    events_log.push_back("completed:" + type_safe::get(e.id));
                }
            },
            ev);
    }};

    const auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);

    // start (Mdiamond) is NON-terminal: emits started+completed.
    // work (box) is NON-terminal: emits started+completed.
    // done (Msquare) is terminal: loop exits before emitting -- no events for terminal node.
    SNITCH_REQUIRE(events_log.size() == 4);
    SNITCH_CHECK(events_log[0] == "started:start");
    SNITCH_CHECK(events_log[1] == "completed:start");
    SNITCH_CHECK(events_log[2] == "started:work");
    SNITCH_CHECK(events_log[3] == "completed:work");

    // Checkpoint written after each node execution.
    SNITCH_CHECK(std::filesystem::exists(logs.path() / "checkpoint.json"));
}
