#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/wait_for_human_handler.hpp>
#include <attractor/interviewer.hpp>
#include <attractor/types.hpp>

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
