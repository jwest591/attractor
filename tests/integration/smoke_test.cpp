#include "attractor_test_support.hpp"

#include <attractor/checkpoint.hpp>
#include <attractor/context.hpp>
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

SNITCH_TEST_CASE("[integration][smoke] resumed run continues log dir numbering from checkpoint -- 7.20-I-001")
{
    // 2-node pipeline: start -> work -> done
    // Run to completion, save a checkpoint with execution_counter = 2,
    // construct a fresh Context, restore counter, run remaining node, verify log dir is 003-...
    auto graph = parse_ok(R"(
        digraph pipeline {
            start [shape=Mdiamond];
            work  [shape=box, prompt="step one"];
            done  [shape=Msquare];
            start -> work;
            work  -> done;
        }
    )");

    TempLogsDir logs;

    // Step 1: Run the full pipeline to build initial state (counter reaches 2 after start+work).
    Engine engine1;
    const auto first_outcome = engine1.run(graph, RunConfig{.logs_root = logs.logs_root()});
    SNITCH_REQUIRE(first_outcome.status == StageStatus::success);

    // Step 2: Manually write a checkpoint that says execution_counter = 2 and next node = work.
    // This simulates a pipeline that was interrupted after start (counter = 1) but before work.
    CheckpointData cp;
    cp.current_node = NodeId{"work"};
    cp.completed_nodes = {NodeId{"start"}};
    cp.execution_counter = 2;
    SNITCH_REQUIRE(save_checkpoint(logs.logs_root(), cp).has_value());

    // Step 3: Resume; engine restores counter = 2 then increments to 3 before dispatching work.
    Engine engine2;
    const auto resume_outcome = engine2.run(graph, RunConfig{.logs_root = logs.logs_root(), .resume = true});
    SNITCH_REQUIRE(resume_outcome.status == StageStatus::success);

    // The work node (dispatched 3rd in this resumed run) must use dir "003-work".
    SNITCH_CHECK(std::filesystem::is_directory(logs.path() / "003-work"));
}
