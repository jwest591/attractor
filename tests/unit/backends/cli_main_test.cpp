#include "cli_utils.hpp"

#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <nlohmann/json.hpp>
#include <snitch/snitch.hpp>
#include "attractor_test_support.hpp"

#include <filesystem>
#include <fstream>

using namespace attractor;
using namespace attractor::test;

SNITCH_TEST_CASE("[cli_main] write_manifest creates manifest.json with all fields -- 7.7-U-001")
{
    TempLogsDir logs;
    Graph g;
    g.digraph_id = GraphId{"my-graph"};

    const bool ok = write_manifest(logs.path().string(), "test-run-id", g);

    SNITCH_REQUIRE(ok);
    const auto manifest_path = logs.path() / "manifest.json";
    SNITCH_REQUIRE(std::filesystem::exists(manifest_path));
    std::ifstream f{manifest_path};
    const auto j = nlohmann::json::parse(f);
    SNITCH_CHECK(j["run_id"].get<std::string>() == "test-run-id");
    SNITCH_CHECK(j.contains("graph_id"));
    SNITCH_CHECK(j.contains("node_count"));
    SNITCH_CHECK(j.contains("edge_count"));
    SNITCH_CHECK(j.contains("run_started"));
}

SNITCH_TEST_CASE("[cli_main] write_manifest node_count and edge_count reflect graph size -- 7.7-U-002")
{
    TempLogsDir logs;
    Graph g;
    g.digraph_id = GraphId{"count-graph"};
    CodergenNode n1;
    n1.id = NodeId{"n1"};
    CodergenNode n2;
    n2.id = NodeId{"n2"};
    g.nodes.push_back(n1);
    g.nodes.push_back(n2);
    Edge e;
    e.from = NodeId{"n1"};
    e.to   = NodeId{"n2"};
    g.edges.push_back(e);

    const bool ok = write_manifest(logs.path().string(), "run-counts", g);
    SNITCH_REQUIRE(ok);

    std::ifstream f{logs.path() / "manifest.json"};
    const auto j = nlohmann::json::parse(f);
    SNITCH_CHECK(j["node_count"].get<std::size_t>() == 2u);
    SNITCH_CHECK(j["edge_count"].get<std::size_t>() == 1u);
}

SNITCH_TEST_CASE("[cli_main] write_manifest run_started is non-empty -- 7.7-U-003")
{
    TempLogsDir logs;
    Graph g;
    g.digraph_id = GraphId{"ts-graph"};

    const bool ok = write_manifest(logs.path().string(), "run-ts", g);
    SNITCH_REQUIRE(ok);

    std::ifstream f{logs.path() / "manifest.json"};
    const auto j = nlohmann::json::parse(f);
    SNITCH_CHECK(!j["run_started"].get<std::string>().empty());
}

SNITCH_TEST_CASE("[cli_main] write_manifest returns false on unwriteable path -- 7.7-U-004")
{
    Graph g;
    g.digraph_id = GraphId{"bad-graph"};

    const bool ok = write_manifest("/proc/nonexistent/bad/path", "run-id", g);

    SNITCH_CHECK(!ok);
}

SNITCH_TEST_CASE("[cli_main] generate_run_id returns non-empty string -- 7.7-U-005")
{
    const std::string id = generate_run_id();

    SNITCH_CHECK(!id.empty());
    SNITCH_CHECK(id.size() >= 18u);
}
