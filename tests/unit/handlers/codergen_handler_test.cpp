#include "attractor_test_support.hpp"

#include <atomic>
#include <attractor/backends/noop_backend.hpp>
#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/codergen_handler.hpp>
#include <attractor/types.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <type_safe/strong_typedef.hpp>

using namespace attractor;

// -- test helpers --------------------------------------------------------------

namespace {

struct ScopedTempDir {
    std::filesystem::path path;

    ScopedTempDir()
    {
        static std::atomic<int> k_counter{0};
        path = std::filesystem::temp_directory_path() / ("att_codergen_test_" + std::to_string(++k_counter));
        std::filesystem::create_directories(path);
    }

    ~ScopedTempDir() { std::filesystem::remove_all(path); }

    ScopedTempDir(const ScopedTempDir&) = delete;
    ScopedTempDir& operator=(const ScopedTempDir&) = delete;
    ScopedTempDir(ScopedTempDir&&) = delete;
    ScopedTempDir& operator=(ScopedTempDir&&) = delete;
};

CodergenNode make_codergen_node(std::string id, std::string prompt_text = "")
{
    CodergenNode n;
    n.id = NodeId{std::move(id)};
    n.shape = NodeShape::box;
    n.prompt = PromptText{std::move(prompt_text)};
    return n;
}

Graph make_graph_with_goal(std::string goal_text)
{
    Graph g;
    g.goal = GoalText{std::move(goal_text)};
    return g;
}

std::string read_file(const std::filesystem::path& p)
{
    std::ifstream f{p};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

class FailingBackend final : public CodergenBackend {
  public:
    [[nodiscard]] auto run(const Node& /*node*/, const PromptText& /*prompt*/,
                           Context& /*ctx*/) const -> std::expected<LlmResponse, Outcome> override
    {
        return std::unexpected(Outcome::fail(DiagnosticMessage{"Simulated backend failure"}));
    }
};

}  // namespace

// -- NoOpBackend tests --------------------------------------------------------

SNITCH_TEST_CASE("[codergen_handler] NoOpBackend::run returns simulated LlmResponse")
{
    NoOpBackend backend;
    CodergenNode n = make_codergen_node("test_node");
    Context ctx;
    Graph g;

    auto result = backend.run(n, PromptText{"some prompt"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(!type_safe::get(*result).empty());
}

SNITCH_TEST_CASE("[codergen_handler] NoOpBackend::run makes no external calls")
{
    // Verifies the simulation contract: run always returns quickly with no I/O.
    // If this test hangs, the NoOpBackend is wrongly calling external systems.
    NoOpBackend backend;
    CodergenNode n = make_codergen_node("any_node");
    Context ctx;
    Graph g;

    auto result = backend.run(n, PromptText{"prompt"}, ctx);
    SNITCH_CHECK(result.has_value());
}

// -- CodergenHandler + NoOpBackend tests --------------------------------------

SNITCH_TEST_CASE("[codergen_handler] execute with NoOpBackend returns SUCCESS -- 2.2-U-003")
{
    ScopedTempDir tmp;
    NoOpBackend backend;
    CodergenHandler h{&backend};
    Context ctx;
    Graph g;
    CodergenNode n = make_codergen_node("plan", "Analyze this");
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(n, ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::success);
}

SNITCH_TEST_CASE("[codergen_handler] execute expands $goal in prompt")
{
    ScopedTempDir tmp;
    NoOpBackend backend;
    CodergenHandler h{&backend};
    Context ctx;
    Graph g = make_graph_with_goal("Write tests");
    CodergenNode n = make_codergen_node("plan", "Plan how to: $goal");
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(n, ctx, g, rc);

    const auto prompt_file = tmp.path / "plan" / "prompt.md";
    SNITCH_REQUIRE(std::filesystem::exists(prompt_file));
    SNITCH_CHECK(read_file(prompt_file) == "Plan how to: Write tests");
}

SNITCH_TEST_CASE("[codergen_handler] execute writes prompt.md to stage directory")
{
    ScopedTempDir tmp;
    NoOpBackend backend;
    CodergenHandler h{&backend};
    Context ctx;
    Graph g;
    CodergenNode n = make_codergen_node("my_stage", "Analyze requirements");
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(n, ctx, g, rc);

    SNITCH_CHECK(std::filesystem::exists(tmp.path / "my_stage" / "prompt.md"));
}

SNITCH_TEST_CASE("[codergen_handler] execute sets last_response in context_updates")
{
    ScopedTempDir tmp;
    NoOpBackend backend;
    CodergenHandler h{&backend};
    Context ctx;
    Graph g;
    CodergenNode n = make_codergen_node("work", "Do something");
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(n, ctx, g, rc);

    SNITCH_REQUIRE(outcome.context_updates.contains("last_response"));
    SNITCH_CHECK(!outcome.context_updates["last_response"].get<std::string>().empty());
}

SNITCH_TEST_CASE("[codergen_handler] execute sets last_stage in context_updates")
{
    ScopedTempDir tmp;
    CodergenHandler h{nullptr};
    Context ctx;
    Graph g;
    CodergenNode n = make_codergen_node("my_work", "Do work");
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(n, ctx, g, rc);

    SNITCH_REQUIRE(outcome.context_updates.contains("last_stage"));
    SNITCH_CHECK(outcome.context_updates["last_stage"].get<std::string>() == "my_work");
}

SNITCH_TEST_CASE("[codergen_handler] null backend uses simulation mode")
{
    ScopedTempDir tmp;
    CodergenHandler h{nullptr};  // simulation mode
    Context ctx;
    Graph g;
    CodergenNode n = make_codergen_node("sim_node", "Do work");
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(n, ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_REQUIRE(outcome.context_updates.contains("last_response"));
    SNITCH_CHECK(!outcome.context_updates["last_response"].get<std::string>().empty());
}

SNITCH_TEST_CASE("[codergen_handler] empty prompt falls back to node label")
{
    ScopedTempDir tmp;
    CodergenHandler h{nullptr};
    Context ctx;
    Graph g;
    CodergenNode n;
    n.id = NodeId{"labeled_node"};
    n.shape = NodeShape::box;
    n.label = NodeLabel{"My fallback label"};
    // n.prompt is empty (default PromptText{})
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(n, ctx, g, rc);

    const auto prompt_file = tmp.path / "labeled_node" / "prompt.md";
    SNITCH_REQUIRE(std::filesystem::exists(prompt_file));
    SNITCH_CHECK(read_file(prompt_file) == "My fallback label");
}

SNITCH_TEST_CASE("[codergen_handler] multiple $goal occurrences all expanded")
{
    ScopedTempDir tmp;
    CodergenHandler h{nullptr};
    Context ctx;
    Graph g = make_graph_with_goal("build app");
    CodergenNode n = make_codergen_node("node1", "First: $goal. Second: $goal.");
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    (void)h.execute(n, ctx, g, rc);

    const auto content = read_file(tmp.path / "node1" / "prompt.md");
    SNITCH_CHECK(content == "First: build app. Second: build app.");
}

SNITCH_TEST_CASE("[codergen_handler] backend failure propagates as Outcome::fail")
{
    ScopedTempDir tmp;
    FailingBackend backend;
    CodergenHandler h{&backend};
    Context ctx;
    Graph g;
    CodergenNode n = make_codergen_node("fail_node", "Do work");
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(n, ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(!type_safe::get(outcome.failure_reason).empty());
}

SNITCH_TEST_CASE("[codergen_handler] node.id with path separator returns Outcome::fail")
{
    ScopedTempDir tmp;
    CodergenHandler h{nullptr};
    Context ctx;
    Graph g;
    CodergenNode n = make_codergen_node("../escape", "prompt");
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(n, ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(!type_safe::get(outcome.failure_reason).empty());
}

SNITCH_TEST_CASE("[codergen_handler] empty prompt and empty label returns Outcome::fail -- 7.7-U-006")
{
    ScopedTempDir tmp;
    CodergenHandler h{nullptr};
    Context ctx;
    Graph g;
    CodergenNode n;
    n.id    = NodeId{"empty_node"};
    n.shape = NodeShape::box;
    RunConfig rc{.logs_root = LogsRoot{tmp.path.string()}};

    auto outcome = h.execute(n, ctx, g, rc);

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(!type_safe::get(outcome.failure_reason).empty());
}
