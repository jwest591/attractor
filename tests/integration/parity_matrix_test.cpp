#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/fan_in_handler.hpp>
#include <attractor/handlers/parallel_handler.hpp>
#include <attractor/handlers/wait_for_human_handler.hpp>
#include <attractor/interviewer.hpp>
#include <attractor/types.hpp>

#include <nlohmann/json.hpp>

using namespace attractor;
using namespace attractor::test;

// Full DoD §11.12 parity matrix (22 cases). Cases are added story by story.
// Remaining cases (to be added in future stories):
//   11.12-001  start node emits START event
//   11.12-002  exit node emits EXIT event with SUCCESS
//   11.12-003  3-node linear pipeline returns SUCCESS
//   11.12-004  conditional branch selects correct edge on context match
//   11.12-005  goal gate unsatisfied routes to retry_target
//   11.12-006  goal gate satisfied exits pipeline with SUCCESS
//   11.12-007  retry with backoff exhausts max_retries and fails
//   11.12-008  checkpoint written after each node
//   11.12-009  resume from checkpoint skips completed nodes
//   11.12-010  tool handler executes shell command and captures output
//   11.12-011  manager_loop cycles until stop condition met
//   11.12-012  wait.human presents choices and routes on selection  <-- implemented below
//   11.12-013  parallel fan-out spawns branches with isolated context
//   11.12-014  parallel fan-in consolidates results by outcome rank
//   11.12-015  model stylesheet applies llm_model by selector
//   11.12-016  fidelity mode compact truncates context correctly
//   11.12-017  AST variable expansion substitutes $goal
//   11.12-018  NoOpBackend returns success for codergen node
//   11.12-019  event stream emits node-complete events in order
//   11.12-020  artifact files written to logs_root/node_id/
//   11.12-021  handler registry resolves custom registered type
//   11.12-022  default handler invoked for unregistered type

SNITCH_TEST_CASE("[parity] Wait.human presents choices and routes on selection -- INT-PARITY")
{
    // DoD §11.12 case 11.12-012: handler executed directly (not through full engine)
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::text, .text = "F"});
    WaitForHumanHandler h{qi};
    Context ctx;
    Graph g;
    Node gate;
    gate.id        = NodeId{"gate"};
    gate.node_type = HandlerTypeName{"wait.human"};
    Edge e1;
    e1.from  = NodeId{"gate"};
    e1.to    = NodeId{"approve"};
    e1.label = EdgeLabel{"[A] Approve"};
    Edge e2;
    e2.from  = NodeId{"gate"};
    e2.to    = NodeId{"fix"};
    e2.label = EdgeLabel{"[F] Fix"};
    g.edges.push_back(e1);
    g.edges.push_back(e2);

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"[F] Fix"});
    SNITCH_CHECK(outcome.context_updates["human.gate.selected"] == "F");
    SNITCH_CHECK(outcome.context_updates["human.gate.label"] == "[F] Fix");
}

SNITCH_TEST_CASE("[parity] Parallel fan-out spawns branches with isolated context -- INT-PARITY")
{
    // DoD 11.12-013: ParallelHandler called directly; branches succeed; parent context unchanged.
    TempLogsDir logs;
    Context ctx;
    (void)ctx.set(ContextKey{"parent.key"}, nlohmann::json{"original"});

    ParallelHandler::RunFn fn = [](const Graph&, const NodeId&, const RunConfig&) -> Outcome {
        return Outcome{.status = StageStatus::success};
    };
    ParallelHandler h{fn};

    Graph g;
    Node par;
    par.id          = NodeId{"par"};
    par.join_policy = JoinPolicy::wait_all;
    par.max_parallel = MaxParallel{4};
    g.nodes.push_back(par);
    Node b0;
    b0.id = NodeId{"b0"};
    g.nodes.push_back(b0);
    Node b1;
    b1.id = NodeId{"b1"};
    g.nodes.push_back(b1);
    Edge e0;
    e0.from = NodeId{"par"};
    e0.to   = NodeId{"b0"};
    g.edges.push_back(e0);
    Edge e1;
    e1.from = NodeId{"par"};
    e1.to   = NodeId{"b1"};
    g.edges.push_back(e1);

    const Node* par_ptr = nullptr;
    for (const auto& n : g.nodes) {
        if (n.id == NodeId{"par"}) {
            par_ptr = &n;
            break;
        }
    }
    SNITCH_REQUIRE(par_ptr != nullptr);

    const Outcome out = h.execute(*par_ptr, ctx, g, logs.logs_root());

    SNITCH_CHECK(out.status == StageStatus::success);
    SNITCH_REQUIRE(out.context_updates.contains("parallel.results"));
    SNITCH_CHECK(out.context_updates["parallel.results"].size() == 2);
    SNITCH_CHECK(ctx.get(ContextKey{"parent.key"}) == nlohmann::json{"original"});
}

SNITCH_TEST_CASE("[parity] Parallel fan-in consolidates results by outcome rank -- INT-PARITY")
{
    // DoD 11.12-014: FanInHandler selects the success candidate over the fail candidate.
    Context ctx;
    nlohmann::json results = nlohmann::json::array();
    results.push_back({{"id","b0"},{"status","fail"},{"score",0.0},{"failure_reason",""},{"notes",""}});
    results.push_back({{"id","b1"},{"status","success"},{"score",0.0},{"failure_reason",""},{"notes",""}});
    (void)ctx.set(ContextKey{"parallel.results"}, results);

    FanInHandler h{nullptr};
    Graph g;
    Node fan_in_node;
    fan_in_node.id = NodeId{"fan_in"};

    const Outcome out = h.execute(fan_in_node, ctx, g, LogsRoot{"/tmp"});

    SNITCH_CHECK(out.status == StageStatus::success);
    SNITCH_REQUIRE(out.context_updates.contains("parallel.fan_in.best_id"));
    SNITCH_CHECK(out.context_updates["parallel.fan_in.best_id"] == "b1");
    SNITCH_REQUIRE(out.context_updates.contains("parallel.fan_in.best_outcome"));
}
