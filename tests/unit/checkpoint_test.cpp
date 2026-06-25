#include "attractor_test_support.hpp"

#include <attractor/checkpoint.hpp>
#include <attractor/types.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <unistd.h>

using namespace attractor;
using namespace attractor::test;

SNITCH_TEST_CASE("[checkpoint] save writes checkpoint.json -- 2.6-P-001")
{
    TempLogsDir logs;

    CheckpointData data;
    data.current_node = NodeId{"nodeA"};
    data.completed_nodes = {NodeId{"start"}};
    data.context = nlohmann::json{{"foo", "bar"}};

    auto result = save_checkpoint(logs.logs_root(), data);
    SNITCH_REQUIRE(result.has_value());

    const auto path = logs.path() / "checkpoint.json";
    SNITCH_CHECK(std::filesystem::exists(path));
}

SNITCH_TEST_CASE("[checkpoint] save and load round-trip all fields -- 2.6-P-002")
{
    TempLogsDir logs;

    CheckpointData data;
    data.timestamp = "2026-01-01T00:00:00Z";
    data.current_node = NodeId{"nodeB"};
    data.completed_nodes = {NodeId{"start"}, NodeId{"nodeA"}};
    data.node_retries = {{"nodeA", 2}};
    data.context = nlohmann::json{{"outcome", "success"}, {"score", 42}};
    data.logs = {"step 1 complete"};

    SNITCH_REQUIRE(save_checkpoint(logs.logs_root(), data).has_value());

    auto loaded = load_checkpoint(logs.logs_root());
    SNITCH_REQUIRE(loaded.has_value());

    SNITCH_CHECK(loaded->timestamp == "2026-01-01T00:00:00Z");
    SNITCH_CHECK(loaded->current_node == NodeId{"nodeB"});
    SNITCH_REQUIRE(loaded->completed_nodes.size() == 2);
    SNITCH_CHECK(loaded->completed_nodes[0] == NodeId{"start"});
    SNITCH_CHECK(loaded->completed_nodes[1] == NodeId{"nodeA"});
    SNITCH_CHECK(loaded->node_retries.at("nodeA") == 2);
    SNITCH_CHECK(loaded->context["outcome"].get<std::string>() == "success");
    SNITCH_CHECK(loaded->context["score"].get<int>() == 42);
    SNITCH_REQUIRE(loaded->logs.size() == 1);
    SNITCH_CHECK(loaded->logs[0] == "step 1 complete");
}

SNITCH_TEST_CASE("[checkpoint] no .tmp file after successful save -- 2.6-P-003")
{
    TempLogsDir logs;

    CheckpointData data;
    data.current_node = NodeId{"x"};

    SNITCH_REQUIRE(save_checkpoint(logs.logs_root(), data).has_value());

    SNITCH_CHECK(!std::filesystem::exists(logs.path() / "checkpoint.json.tmp"));
    SNITCH_CHECK(!std::filesystem::exists(
        logs.path() / ("checkpoint.json.tmp." + std::to_string(getpid()))));
}

SNITCH_TEST_CASE("[checkpoint] load returns error for missing file -- 2.6-P-004")
{
    TempLogsDir logs;
    auto result = load_checkpoint(logs.logs_root());
    SNITCH_CHECK(!result.has_value());
}

SNITCH_TEST_CASE("[checkpoint] load returns error for malformed JSON -- 2.6-P-005")
{
    TempLogsDir logs;

    const auto path = logs.path() / "checkpoint.json";
    {
        std::ofstream f{path};
        f << "this is not json {{{";
    }

    auto result = load_checkpoint(logs.logs_root());
    SNITCH_CHECK(!result.has_value());
}

SNITCH_TEST_CASE("[checkpoint] save_checkpoint uses pid-unique temp filename -- 7.9-CP-001")
{
    TempLogsDir logs;

    CheckpointData data;
    data.current_node = NodeId{"nodeA"};

    SNITCH_REQUIRE(save_checkpoint(logs.logs_root(), data).has_value());

    SNITCH_CHECK(!std::filesystem::exists(logs.path() / "checkpoint.json.tmp"));
    SNITCH_CHECK(!std::filesystem::exists(
        logs.path() / ("checkpoint.json.tmp." + std::to_string(getpid()))));
}

SNITCH_TEST_CASE("[checkpoint] execution_counter round-trips through save and load -- 7.20-U-001")
{
    TempLogsDir logs;

    CheckpointData data;
    data.current_node = NodeId{"nodeX"};
    data.execution_counter = 7;

    SNITCH_REQUIRE(save_checkpoint(logs.logs_root(), data).has_value());

    auto loaded = load_checkpoint(logs.logs_root());
    SNITCH_REQUIRE(loaded.has_value());
    SNITCH_CHECK(loaded->execution_counter == 7);
}
