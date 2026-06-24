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

ToolNode make_tool_node(std::string id, std::string cmd)
{
    ToolNode n;
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
    SNITCH_CHECK(outcome.context_updates["tool"]["output"].get<std::string>() == "hello");
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

SNITCH_TEST_CASE("[tool_handler] creates invocation subdirectory on success")
{
    ScopedTempDir tmp;
    ToolHandler h{[](std::string_view) { return "out"; }};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("my_tool", "cmd"), ctx, g, rc);

    SNITCH_CHECK(std::filesystem::is_directory(tmp.path / "my_tool" / "001"));
}

SNITCH_TEST_CASE("[tool_handler] second invocation creates 002 subdirectory")
{
    ScopedTempDir tmp;
    ToolHandler h{[](std::string_view) { return "out"; }};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("my_tool", "cmd"), ctx, g, rc);
    (void)h.execute(make_tool_node("my_tool", "cmd"), ctx, g, rc);

    SNITCH_CHECK(std::filesystem::is_directory(tmp.path / "my_tool" / "001"));
    SNITCH_CHECK(std::filesystem::is_directory(tmp.path / "my_tool" / "002"));
}

SNITCH_TEST_CASE("[tool_handler] writes command.txt with expanded command")
{
    ScopedTempDir tmp;
    ToolHandler h{[](std::string_view) { return "out"; }};
    Context ctx;
    Graph g;
    g.goal = GoalText{"7.12"};
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("run_tests", "status.sh $goal"), ctx, g, rc);

    const auto cmd_file = tmp.path / "run_tests" / "001" / "command.txt";
    SNITCH_REQUIRE(std::filesystem::exists(cmd_file));
    SNITCH_CHECK(read_file(cmd_file) == "status.sh 7.12");
}

SNITCH_TEST_CASE("[tool_handler] writes command.txt on empty-command failure")
{
    ScopedTempDir tmp;
    ToolHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("bad_tool", ""), ctx, g, rc);

    const auto cmd_file = tmp.path / "bad_tool" / "001" / "command.txt";
    SNITCH_REQUIRE(std::filesystem::exists(cmd_file));
    SNITCH_CHECK(read_file(cmd_file).empty());
}

SNITCH_TEST_CASE("[tool_handler] writes output.txt with command stdout")
{
    ScopedTempDir tmp;
    ToolHandler h{[](std::string_view) { return "line1\nline2\n"; }};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("run_tests", "cmd"), ctx, g, rc);

    const auto out_file = tmp.path / "run_tests" / "001" / "output.txt";
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

    const auto status_file = tmp.path / "deploy" / "001" / "status.json";
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

    const auto status_file = tmp.path / "bad_tool" / "001" / "status.json";
    SNITCH_REQUIRE(std::filesystem::exists(status_file));

    const auto j = nlohmann::json::parse(read_file(status_file));
    SNITCH_CHECK(j["outcome"].get<std::string>() == "fail");
    SNITCH_CHECK(!j["failure_reason"].get<std::string>().empty());
}

SNITCH_TEST_CASE("[tool_handler] trailing newlines stripped from context output")
{
    ScopedTempDir tmp;
    ToolHandler h{[](std::string_view) { return "done\n"; }};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(make_tool_node("check", "cmd"), ctx, g, rc);

    SNITCH_REQUIRE(outcome.context_updates.contains("tool"));
    SNITCH_CHECK(outcome.context_updates["tool"]["output"].get<std::string>() == "done");
}

SNITCH_TEST_CASE("[tool_handler] output.txt preserves raw bytes including newline")
{
    ScopedTempDir tmp;
    ToolHandler h{[](std::string_view) { return "done\n"; }};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(make_tool_node("check", "cmd"), ctx, g, rc);

    SNITCH_CHECK(read_file(tmp.path / "check" / "001" / "output.txt") == "done\n");
}

SNITCH_TEST_CASE("[tool_handler] $goal in tool_command is expanded")
{
    ScopedTempDir tmp;
    std::string captured_cmd;
    ToolHandler h{[&](std::string_view cmd) -> std::string {
        captured_cmd = std::string(cmd);
        return "done";
    }};
    Context ctx;
    Graph g;
    g.goal = GoalText{"7.11"};
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    ToolNode n = make_tool_node("check", "status.sh $goal --config cfg.yaml");
    (void)h.execute(n, ctx, g, rc);

    SNITCH_CHECK(captured_cmd == "status.sh 7.11 --config cfg.yaml");
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

SNITCH_TEST_CASE("[tool_handler] real popen: stderr is captured to stderr.txt")
{
    ScopedTempDir tmp;
    ToolHandler h;  // no injected runner -- uses real popen
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    // Command writes distinct strings to stdout and stderr.
    auto outcome = h.execute(make_tool_node("real_run", "printf 'out' && printf 'err' >&2"), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(read_file(tmp.path / "real_run" / "001" / "output.txt") == "out");
    SNITCH_CHECK(read_file(tmp.path / "real_run" / "001" / "stderr.txt") == "err");
}
