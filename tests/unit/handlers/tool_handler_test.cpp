#include "attractor_test_support.hpp"

#include <atomic>
#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/tool_handler.hpp>
#include <attractor/types.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <type_safe/strong_typedef.hpp>

using namespace attractor;

namespace {

struct ScopedTempDir {
    std::filesystem::path path;

    ScopedTempDir()
    {
        static std::atomic<int> k_counter{0};
        path = std::filesystem::temp_directory_path() / ("att_tool_test_" + std::to_string(++k_counter));
        std::filesystem::create_directories(path);
    }

    ~ScopedTempDir() { std::filesystem::remove_all(path); }

    ScopedTempDir(const ScopedTempDir&) = delete;
    ScopedTempDir& operator=(const ScopedTempDir&) = delete;
    ScopedTempDir(ScopedTempDir&&) = delete;
    ScopedTempDir& operator=(ScopedTempDir&&) = delete;
};

Node make_tool_node(std::string id, std::string cmd)
{
    Node n;
    n.id = NodeId{std::move(id)};
    n.shape = NodeShape::parallelogram;
    n.tool_command = ShellCommand{std::move(cmd)};
    return n;
}

std::string read_file(const std::filesystem::path& p)
{
    std::ifstream f{p};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

SNITCH_TEST_CASE("[tool_handler] injected runner returns output -- 2.8-U-001")
{
    ScopedTempDir tmp;
    ToolHandler::CommandRunner runner = [](std::string_view /*cmd*/) -> std::string {
        return "hello\n";
    };
    ToolHandler h{std::move(runner)};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(make_tool_node("my_tool", "echo hello"), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_REQUIRE(outcome.context_updates.contains("tool"));
    SNITCH_CHECK(outcome.context_updates["tool"]["output"].get<std::string>() == "hello\n");
}

SNITCH_TEST_CASE("[tool_handler] empty tool_command returns FAIL -- 2.8-U-002")
{
    ScopedTempDir tmp;
    ToolHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(make_tool_node("bad_tool", ""), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(!type_safe::get(outcome.failure_reason).empty());
}

SNITCH_TEST_CASE("[tool_handler] injected runner called with correct command -- 2.8-U-003")
{
    ScopedTempDir tmp;
    int call_count = 0;
    std::string captured_cmd;

    ToolHandler::CommandRunner runner = [&](std::string_view cmd) -> std::string {
        ++call_count;
        captured_cmd = std::string(cmd);
        return "output";
    };
    ToolHandler h{std::move(runner)};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("tool", "ls -la"), ctx, g, rc);

    SNITCH_CHECK(call_count == 1);
    SNITCH_CHECK(captured_cmd == "ls -la");
}

SNITCH_TEST_CASE("[tool_handler] creates node log directory on success")
{
    ScopedTempDir tmp;
    ToolHandler h{[](std::string_view) { return "out"; }};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("my_tool", "cmd"), ctx, g, rc);

    SNITCH_CHECK(std::filesystem::is_directory(tmp.path / "my_tool"));
}

SNITCH_TEST_CASE("[tool_handler] writes output.txt with command stdout")
{
    ScopedTempDir tmp;
    ToolHandler h{[](std::string_view) { return "line1\nline2\n"; }};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("run_tests", "cmd"), ctx, g, rc);

    const auto out_file = tmp.path / "run_tests" / "output.txt";
    SNITCH_REQUIRE(std::filesystem::exists(out_file));
    SNITCH_CHECK(read_file(out_file) == "line1\nline2\n");
}

SNITCH_TEST_CASE("[tool_handler] writes status.json with success outcome")
{
    ScopedTempDir tmp;
    ToolHandler h{[](std::string_view) { return "done"; }};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("deploy", "cmd"), ctx, g, rc);

    const auto status_file = tmp.path / "deploy" / "status.json";
    SNITCH_REQUIRE(std::filesystem::exists(status_file));

    const auto j = nlohmann::json::parse(read_file(status_file));
    SNITCH_CHECK(j.contains("outcome"));
    SNITCH_CHECK(j["outcome"].get<std::string>() == "success");
}

SNITCH_TEST_CASE("[tool_handler] writes status.json on empty-command failure")
{
    ScopedTempDir tmp;
    ToolHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("bad_tool", ""), ctx, g, rc);

    const auto status_file = tmp.path / "bad_tool" / "status.json";
    SNITCH_REQUIRE(std::filesystem::exists(status_file));

    const auto j = nlohmann::json::parse(read_file(status_file));
    SNITCH_CHECK(j["outcome"].get<std::string>() == "fail");
    SNITCH_CHECK(!j["failure_reason"].get<std::string>().empty());
}

SNITCH_TEST_CASE("[tool_handler] node.id with path separator returns FAIL")
{
    ScopedTempDir tmp;
    ToolHandler h{[](std::string_view) { return ""; }};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(make_tool_node("../escape", "cmd"), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(!type_safe::get(outcome.failure_reason).empty());
}
