#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/parallel_handler.hpp>
#include <attractor/types.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <limits>
#include <thread>

using namespace attractor;

namespace {

Graph make_parallel_graph(JoinPolicy join_policy = JoinPolicy::wait_all, int max_parallel = 4)
{
    Graph g;
    ParallelNode par;
    par.id = NodeId{"par"};
    par.join_policy = join_policy;
    par.max_parallel = MaxParallel{max_parallel};
    g.nodes.push_back(par);

    CodergenNode b0;
    b0.id = NodeId{"b0"};
    g.nodes.push_back(b0);

    CodergenNode b1;
    b1.id = NodeId{"b1"};
    g.nodes.push_back(b1);

    Edge e0;
    e0.from = NodeId{"par"};
    e0.to = NodeId{"b0"};
    g.edges.push_back(e0);

    Edge e1;
    e1.from = NodeId{"par"};
    e1.to = NodeId{"b1"};
    g.edges.push_back(e1);

    return g;
}

Graph make_parallel_graph_n(int n, JoinPolicy join_policy = JoinPolicy::wait_all, int max_parallel = 4)
{
    SNITCH_REQUIRE(n > 0);
    Graph g;
    ParallelNode par;
    par.id = NodeId{"par"};
    par.join_policy = join_policy;
    par.max_parallel = MaxParallel{max_parallel};
    g.nodes.push_back(par);

    for (int i = 0; i < n; ++i) {
        CodergenNode b;
        b.id = NodeId{"b" + std::to_string(i)};
        g.nodes.push_back(b);

        Edge e;
        e.from = NodeId{"par"};
        e.to = NodeId{"b" + std::to_string(i)};
        g.edges.push_back(e);
    }

    return g;
}

const ParallelNode& find_par_node(const Graph& g)
{
    auto it = std::ranges::find_if(g.nodes, [](const NodeVariant& nv) {
        return to_base(nv).id == NodeId{"par"};
    });
    assert(it != g.nodes.end());
    return std::get<ParallelNode>(*it);
}

}  // namespace

SNITCH_TEST_CASE("[parallel_handler] wait_all both branches succeed returns SUCCESS -- 4.1-U-001")
{
    ParallelHandler::RunFn always_succeed = [](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        return Outcome{.status = StageStatus::success};
    };
    ParallelHandler h{always_succeed};
    Context ctx;
    const Graph g = make_parallel_graph();
    const ParallelNode& par = find_par_node(g);

    const Outcome out = h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(out.status == StageStatus::success);
    SNITCH_REQUIRE(out.context_updates.contains("parallel.results"));
    SNITCH_CHECK(out.context_updates["parallel.results"].is_array());
    SNITCH_CHECK(out.context_updates["parallel.results"].size() == 2);
}

SNITCH_TEST_CASE("[parallel_handler] wait_all one branch fails returns PARTIAL_SUCCESS -- 4.1-U-002")
{
    std::atomic<int> call{0};
    ParallelHandler::RunFn mixed_fn = [&call](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        const int n = call++;
        if (n == 0)
            return Outcome{.status = StageStatus::success};
        return Outcome::fail(DiagnosticMessage{"branch 1 failed"});
    };
    ParallelHandler h{mixed_fn};
    Context ctx;
    const Graph g = make_parallel_graph();
    const ParallelNode& par = find_par_node(g);

    const Outcome out = h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(out.status == StageStatus::partial_success);
    SNITCH_REQUIRE(out.context_updates.contains("parallel.results"));
    SNITCH_CHECK(out.context_updates["parallel.results"].size() == 2);
}

SNITCH_TEST_CASE("[parallel_handler] first_success one branch succeeds returns SUCCESS -- 4.1-U-003")
{
    std::atomic<int> call{0};
    ParallelHandler::RunFn mixed_fn = [&call](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        const int n = call++;
        if (n == 0)
            return Outcome{.status = StageStatus::success};
        return Outcome::fail(DiagnosticMessage{"branch 1 failed"});
    };
    ParallelHandler h{mixed_fn};
    Context ctx;
    const Graph g = make_parallel_graph(JoinPolicy::first_success);
    const ParallelNode& par = find_par_node(g);

    const Outcome out = h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(out.status == StageStatus::success);
}

SNITCH_TEST_CASE("[parallel_handler] first_success all branches fail returns FAIL -- 4.1-U-004")
{
    ParallelHandler::RunFn always_fail = [](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        return Outcome::fail(DiagnosticMessage{"branch failed"});
    };
    ParallelHandler h{always_fail};
    Context ctx;
    const Graph g = make_parallel_graph(JoinPolicy::first_success);
    const ParallelNode& par = find_par_node(g);

    const Outcome out = h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(out.status == StageStatus::fail);
}

SNITCH_TEST_CASE("[parallel_handler] parent context unchanged after fan-out -- 4.1-U-005")
{
    ParallelHandler::RunFn no_op = [](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        return Outcome{};
    };
    ParallelHandler h{no_op};
    Context ctx;
    (void)ctx.set(ContextKey{"parent.key"}, nlohmann::json{"original"});
    const Graph g = make_parallel_graph();
    const ParallelNode& par = find_par_node(g);

    (void)h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(ctx.get(ContextKey{"parent.key"}) == nlohmann::json{"original"});
}

SNITCH_TEST_CASE("[parallel_handler] max_parallel limits concurrent branch count -- 4.1-U-006")
{
    std::atomic<int> active{0};
    std::atomic<int> peak{0};
    ParallelHandler::RunFn counting_fn = [&active, &peak](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        const int cur = ++active;
        int expected = peak.load();
        while (cur > expected && !peak.compare_exchange_weak(expected, cur)) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        --active;
        return Outcome{};
    };
    ParallelHandler h{counting_fn};
    Context ctx;
    const Graph g = make_parallel_graph_n(4, JoinPolicy::wait_all, 2);
    const ParallelNode& par = find_par_node(g);

    (void)h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(peak.load() <= 2);
}

SNITCH_TEST_CASE("[parallel_handler] no outgoing branches returns FAIL -- 4.1-U-007")
{
    ParallelHandler::RunFn unreachable = [](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        return Outcome{};
    };
    ParallelHandler h{unreachable};
    Context ctx;
    Graph g;
    ParallelNode par;
    par.id = NodeId{"par"};
    par.join_policy = JoinPolicy::wait_all;
    par.max_parallel = MaxParallel{4};
    g.nodes.push_back(par);

    const Outcome out = h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(out.status == StageStatus::fail);
    SNITCH_CHECK(!type_safe::get(out.failure_reason).empty());
}

SNITCH_TEST_CASE("[parallel_handler] callable through Handler interface -- 4.1-U-008")
{
    ParallelHandler::RunFn always_succeed = [](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        return Outcome{.status = StageStatus::success};
    };
    ParallelHandler h{always_succeed};
    Handler& iface = h;
    Context ctx;
    const Graph g = make_parallel_graph();
    const ParallelNode& par = find_par_node(g);

    const Outcome out = iface.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(out.status == StageStatus::success);
}

SNITCH_TEST_CASE("[parallel_handler] results entries contain id and score fields -- 4.2-U-P01")
{
    ParallelHandler::RunFn fn = [](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        return Outcome{.status = StageStatus::success};
    };
    ParallelHandler h{fn};
    Context ctx;
    const Graph g = make_parallel_graph();
    const ParallelNode& par = find_par_node(g);

    const Outcome out = h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_REQUIRE(out.context_updates.contains("parallel.results"));
    const auto& results = out.context_updates["parallel.results"];
    SNITCH_REQUIRE(results.size() == 2);
    for (const auto& entry : results) {
        SNITCH_CHECK(entry.contains("id"));
        SNITCH_CHECK(entry.contains("score"));
        SNITCH_CHECK(entry["score"].is_number());
    }
}

SNITCH_TEST_CASE("[parallel_handler] first_success cancels unstarted branches -- 7.10-U-001")
{
    std::atomic<int> run_count{0};
    ParallelHandler::RunFn fn = [&run_count](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        ++run_count;
        return Outcome{.status = StageStatus::success};
    };
    ParallelHandler h{fn};
    Context ctx;
    const Graph g = make_parallel_graph_n(2, JoinPolicy::first_success, 1);
    const ParallelNode& par = find_par_node(g);

    const Outcome out = h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(out.status == StageStatus::success);
    SNITCH_CHECK(run_count.load() == 1);
}

SNITCH_TEST_CASE("[parallel_handler] retry_policy propagated to branch RunFn config -- 7.10-U-002")
{
    RunConfig captured;
    ParallelHandler::RunFn fn = [&captured](const Graph&, const NodeId&, const RunConfig& rc) -> Outcome {
        captured = rc;
        return Outcome{.status = StageStatus::success};
    };
    ParallelHandler h{fn};
    Context ctx;
    const Graph g = make_parallel_graph_n(1, JoinPolicy::wait_all, 1);
    const ParallelNode& par = find_par_node(g);
    const RunConfig parent_config{
        .logs_root    = LogsRoot{"/tmp"},
        .retry_policy = RetryPolicy{.preset = BackoffPreset::fixed_1s, .sleep_fn = {}}
    };

    (void)h.execute(par, ctx, g, parent_config);

    SNITCH_CHECK(captured.retry_policy.preset == BackoffPreset::fixed_1s);
}

SNITCH_TEST_CASE("[parallel_handler] non-finite score in context_updates returns FAIL -- 7.10-U-003")
{
    ParallelHandler::RunFn fn = [](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        Outcome o{.status = StageStatus::success};
        o.context_updates["parallel.score"] = std::numeric_limits<double>::quiet_NaN();
        return o;
    };
    ParallelHandler h{fn};
    Context ctx;
    const Graph g = make_parallel_graph();
    const ParallelNode& par = find_par_node(g);

    const Outcome out = h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(out.status == StageStatus::fail);
    SNITCH_CHECK(!type_safe::get(out.failure_reason).empty());
}

SNITCH_TEST_CASE("[parallel_handler] first_success succeeds when in-flight branch has NaN score -- 7.10-U-005")
{
    // Both branches run simultaneously (max_parallel=2). b1 returns NaN score.
    // With first_success_found=true after b0 wins, the NaN from b1 must not kill the outcome.
    std::atomic<int> started{0};
    ParallelHandler::RunFn fn = [&started](const Graph&, const NodeId& id, const RunConfig&) -> Outcome {
        ++started;
        while (started.load(std::memory_order_acquire) < 2)
            std::this_thread::yield();
        Outcome o{.status = StageStatus::success};
        if (type_safe::get(id) == "b1")
            o.context_updates["parallel.score"] = std::numeric_limits<double>::quiet_NaN();
        return o;
    };
    ParallelHandler h{fn};
    Context ctx;
    const Graph g = make_parallel_graph_n(2, JoinPolicy::first_success, 2);
    const ParallelNode& par = find_par_node(g);

    const Outcome out = h.execute(par, ctx, g, RunConfig{.logs_root = LogsRoot{"/tmp"}});

    SNITCH_CHECK(out.status == StageStatus::success);
}
