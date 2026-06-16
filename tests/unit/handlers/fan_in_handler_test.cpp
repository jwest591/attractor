#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/fan_in_handler.hpp>
#include <attractor/types.hpp>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

using namespace attractor;

namespace {

void set_results(Context& ctx, nlohmann::json results)
{
    (void)ctx.set(ContextKey{"parallel.results"}, std::move(results));
}

nlohmann::json make_entry(const std::string& id, const std::string& status, double score = 0.0)
{
    return nlohmann::json{
        {"id",             id},
        {"status",         status},
        {"score",          score},
        {"failure_reason", ""},
        {"notes",          ""}
    };
}

class SpyBackend final : public CodergenBackend {
  public:
    mutable int call_count{0};
    mutable std::string last_prompt;
    std::string return_id;

    [[nodiscard]] auto run(const Node& /*node*/, const PromptText& prompt, Context& /*ctx*/) const
        -> std::expected<LlmResponse, Outcome> override
    {
        ++call_count;
        last_prompt = type_safe::get(prompt);
        return LlmResponse{return_id};
    }
};

}  // namespace

SNITCH_TEST_CASE("[fan_in_handler] one SUCCESS and one FAIL returns SUCCESS with correct best_id -- 4.2-U-001")
{
    FanInHandler h{nullptr};
    Context ctx;
    nlohmann::json results = nlohmann::json::array();
    results.push_back(make_entry("b0", "success"));
    results.push_back(make_entry("b1", "fail"));
    set_results(ctx, results);
    Graph g;
    Node par;
    par.id = NodeId{"fan_in"};

    const Outcome out = h.execute(par, ctx, g, LogsRoot{"/tmp"});

    SNITCH_CHECK(out.status == StageStatus::success);
    SNITCH_REQUIRE(out.context_updates.contains("parallel.fan_in.best_id"));
    SNITCH_CHECK(out.context_updates["parallel.fan_in.best_id"] == "b0");
    SNITCH_REQUIRE(out.context_updates.contains("parallel.fan_in.best_outcome"));
    SNITCH_CHECK(out.context_updates["parallel.fan_in.best_outcome"].is_object());
}

SNITCH_TEST_CASE("[fan_in_handler] two SUCCESS with different scores selects higher score -- 4.2-U-002")
{
    FanInHandler h{nullptr};
    Context ctx;
    nlohmann::json results = nlohmann::json::array();
    results.push_back(make_entry("b0", "success", 10.0));
    results.push_back(make_entry("b1", "success", 5.0));
    set_results(ctx, results);
    Graph g;
    Node par;
    par.id = NodeId{"fan_in"};

    const Outcome out = h.execute(par, ctx, g, LogsRoot{"/tmp"});

    SNITCH_CHECK(out.status == StageStatus::success);
    SNITCH_REQUIRE(out.context_updates.contains("parallel.fan_in.best_id"));
    SNITCH_CHECK(out.context_updates["parallel.fan_in.best_id"] == "b0");
}

SNITCH_TEST_CASE("[fan_in_handler] two SUCCESS equal score tiebreaks on id ascending -- 4.2-U-003")
{
    FanInHandler h{nullptr};
    Context ctx;
    nlohmann::json results = nlohmann::json::array();
    results.push_back(make_entry("b0", "success", 0.0));
    results.push_back(make_entry("b1", "success", 0.0));
    set_results(ctx, results);
    Graph g;
    Node par;
    par.id = NodeId{"fan_in"};

    const Outcome out = h.execute(par, ctx, g, LogsRoot{"/tmp"});

    SNITCH_CHECK(out.status == StageStatus::success);
    SNITCH_REQUIRE(out.context_updates.contains("parallel.fan_in.best_id"));
    SNITCH_CHECK(out.context_updates["parallel.fan_in.best_id"] == "b0");
}

SNITCH_TEST_CASE("[fan_in_handler] all candidates FAIL returns FAIL -- 4.2-U-004")
{
    FanInHandler h{nullptr};
    Context ctx;
    nlohmann::json results = nlohmann::json::array();
    results.push_back(make_entry("b0", "fail"));
    results.push_back(make_entry("b1", "fail"));
    set_results(ctx, results);
    Graph g;
    Node par;
    par.id = NodeId{"fan_in"};

    const Outcome out = h.execute(par, ctx, g, LogsRoot{"/tmp"});

    SNITCH_CHECK(out.status == StageStatus::fail);
}

SNITCH_TEST_CASE("[fan_in_handler] no parallel.results key in context returns FAIL -- 4.2-U-005")
{
    FanInHandler h{nullptr};
    Context ctx;
    Graph g;
    Node par;
    par.id = NodeId{"fan_in"};

    const Outcome out = h.execute(par, ctx, g, LogsRoot{"/tmp"});

    SNITCH_CHECK(out.status == StageStatus::fail);
    SNITCH_CHECK(!type_safe::get(out.failure_reason).empty());
}

SNITCH_TEST_CASE("[fan_in_handler] prompt and backend causes backend to be called and its id used -- 4.2-U-006")
{
    SpyBackend spy;
    spy.return_id = "b1";
    FanInHandler h{&spy};
    Context ctx;
    nlohmann::json results = nlohmann::json::array();
    results.push_back(make_entry("b0", "success", 0.0));
    results.push_back(make_entry("b1", "success", 0.0));
    set_results(ctx, results);
    Graph g;
    Node par;
    par.id    = NodeId{"fan_in"};
    par.prompt = PromptText{"select the best candidate"};

    const Outcome out = h.execute(par, ctx, g, LogsRoot{"/tmp"});

    SNITCH_CHECK(out.status == StageStatus::success);
    SNITCH_CHECK(spy.call_count == 1);
    SNITCH_REQUIRE(out.context_updates.contains("parallel.fan_in.best_id"));
    SNITCH_CHECK(out.context_updates["parallel.fan_in.best_id"] == "b1");
}

SNITCH_TEST_CASE("[fan_in_handler] partial_success vs success selects success by rank -- 4.2-U-007")
{
    FanInHandler h{nullptr};
    Context ctx;
    nlohmann::json results = nlohmann::json::array();
    results.push_back(make_entry("b0", "partial_success", 0.0));
    results.push_back(make_entry("b1", "success", 0.0));
    set_results(ctx, results);
    Graph g;
    Node par;
    par.id = NodeId{"fan_in"};

    const Outcome out = h.execute(par, ctx, g, LogsRoot{"/tmp"});

    SNITCH_CHECK(out.status == StageStatus::success);
    SNITCH_REQUIRE(out.context_updates.contains("parallel.fan_in.best_id"));
    SNITCH_CHECK(out.context_updates["parallel.fan_in.best_id"] == "b1");
}

SNITCH_TEST_CASE("[fan_in_handler] callable through Handler interface -- 4.2-U-008")
{
    FanInHandler h{nullptr};
    Handler& iface = h;
    Context ctx;
    nlohmann::json results = nlohmann::json::array();
    results.push_back(make_entry("b0", "success", 0.0));
    set_results(ctx, results);
    Graph g;
    Node par;
    par.id = NodeId{"fan_in"};

    const Outcome out = iface.execute(par, ctx, g, LogsRoot{"/tmp"});

    SNITCH_CHECK(out.status == StageStatus::success);
    SNITCH_REQUIRE(out.context_updates.contains("parallel.fan_in.best_id"));
    SNITCH_CHECK(out.context_updates["parallel.fan_in.best_id"] == "b0");
}
