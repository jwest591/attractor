#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/tool_handler.hpp>
#include <attractor/types.hpp>
#include <string>
#include <string_view>

using namespace attractor;

namespace {

Node make_tool_node(std::string id, std::string cmd)
{
    Node n;
    n.id = NodeId{std::move(id)};
    n.shape = NodeShape::parallelogram;
    n.tool_command = ShellCommand{std::move(cmd)};
    return n;
}

}  // namespace

SNITCH_TEST_CASE("[tool_handler] injected runner returns output -- 2.8-U-001")
{
    ToolHandler::CommandRunner runner = [](std::string_view /*cmd*/) -> std::string {
        return "hello\n";
    };
    ToolHandler h{std::move(runner)};
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    auto outcome = h.execute(make_tool_node("my_tool", "echo hello"), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_REQUIRE(outcome.context_updates.contains("tool_output"));
    SNITCH_CHECK(outcome.context_updates["tool_output"].get<std::string>() == "hello\n");
}

SNITCH_TEST_CASE("[tool_handler] empty tool_command returns FAIL -- 2.8-U-002")
{
    ToolHandler h;
    Context ctx;
    Graph g;
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    auto outcome = h.execute(make_tool_node("bad_tool", ""), ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(!type_safe::get(outcome.failure_reason).empty());
}

SNITCH_TEST_CASE("[tool_handler] injected runner called with correct command -- 2.8-U-003")
{
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
    RunConfig rc{.logs_root = LogsRoot{"./logs"}};

    (void)h.execute(make_tool_node("tool", "ls -la"), ctx, g, rc);

    SNITCH_CHECK(call_count == 1);
    SNITCH_CHECK(captured_cmd == "ls -la");
}
