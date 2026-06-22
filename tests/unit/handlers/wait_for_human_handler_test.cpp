#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handlers/wait_for_human_handler.hpp>
#include <attractor/interviewer.hpp>
#include <attractor/types.hpp>
#include <string>

using namespace attractor;
using namespace attractor::test;

namespace {

Graph make_approve_fix_graph()
{
    Graph g;
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
    return g;
}

Node make_gate_node(std::string id = "gate")
{
    Node n;
    n.id        = NodeId{std::move(id)};
    n.node_type = HandlerTypeName{"wait.human"};
    return n;
}

Graph make_single_edge_graph(const std::string& from_id, const std::string& to_id,
                              const std::string& edge_label)
{
    Graph g;
    Edge e;
    e.from  = NodeId{from_id};
    e.to    = NodeId{to_id};
    e.label = EdgeLabel{edge_label};
    g.edges.push_back(e);
    return g;
}

}  // namespace

SNITCH_TEST_CASE("[wait_for_human_handler] QueueInterviewer routes to approve branch on key A -- 3.2-U-001")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::text, .text = "A"});
    WaitForHumanHandler h{qi};
    Context ctx;
    auto g    = make_approve_fix_graph();
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"[A] Approve"});
    SNITCH_CHECK(outcome.context_updates["human.gate.selected"] == "A");
    SNITCH_CHECK(outcome.context_updates["human.gate.label"] == "[A] Approve");
}

SNITCH_TEST_CASE("[wait_for_human_handler] bracket format [Y] Yes extracts key Y -- 3.2-U-002")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::text, .text = "Y"});
    WaitForHumanHandler h{qi};
    Context ctx;
    auto g    = make_single_edge_graph("gate", "yes", "[Y] Yes");
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"[Y] Yes"});
    SNITCH_CHECK(outcome.context_updates["human.gate.selected"] == "Y");
    SNITCH_CHECK(outcome.context_updates["human.gate.label"] == "[Y] Yes");
}

SNITCH_TEST_CASE("[wait_for_human_handler] paren format Y) Yes extracts key Y -- 3.2-U-003")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::text, .text = "Y"});
    WaitForHumanHandler h{qi};
    Context ctx;
    auto g    = make_single_edge_graph("gate", "yes", "Y) Yes");
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"Y) Yes"});
    SNITCH_CHECK(outcome.context_updates["human.gate.selected"] == "Y");
    SNITCH_CHECK(outcome.context_updates["human.gate.label"] == "Y) Yes");
}

SNITCH_TEST_CASE("[wait_for_human_handler] dash format Y - Yes extracts key Y -- 3.2-U-004")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::text, .text = "Y"});
    WaitForHumanHandler h{qi};
    Context ctx;
    auto g    = make_single_edge_graph("gate", "yes", "Y - Yes");
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"Y - Yes"});
    SNITCH_CHECK(outcome.context_updates["human.gate.selected"] == "Y");
    SNITCH_CHECK(outcome.context_updates["human.gate.label"] == "Y - Yes");
}

SNITCH_TEST_CASE("[wait_for_human_handler] no-format label falls back to first char -- 3.2-U-005")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::text, .text = "Y"});
    WaitForHumanHandler h{qi};
    Context ctx;
    auto g    = make_single_edge_graph("gate", "yes", "Yes");
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"Yes"});
    SNITCH_CHECK(outcome.context_updates["human.gate.selected"] == "Y");
    SNITCH_CHECK(outcome.context_updates["human.gate.label"] == "Yes");
}

SNITCH_TEST_CASE("[wait_for_human_handler] timeout with human.default_choice routes to default -- 3.2-U-006")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::timeout});
    WaitForHumanHandler h{qi};
    Context ctx;
    auto g    = make_approve_fix_graph();
    auto gate = make_gate_node("gate");
    gate.human_default_choice = NodeId{"approve"};

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"[A] Approve"});
}

SNITCH_TEST_CASE("[wait_for_human_handler] timeout with no default choice returns RETRY -- 3.2-U-007")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::timeout});
    WaitForHumanHandler h{qi};
    Context ctx;
    auto g    = make_approve_fix_graph();
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::retry);
}

SNITCH_TEST_CASE("[wait_for_human_handler] no outgoing edges returns FAIL -- 3.2-U-008")
{
    TempLogsDir logs;
    AutoApproveInterviewer ai;
    WaitForHumanHandler h{ai};
    Context ctx;
    Graph g;
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(type_safe::get(outcome.failure_reason).find("No outgoing edges") != std::string::npos);
}

SNITCH_TEST_CASE("[wait_for_human_handler] skipped answer returns FAIL -- 3.2-U-009")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::skipped});
    WaitForHumanHandler h{qi};
    Context ctx;
    auto g    = make_approve_fix_graph();
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::fail);
    SNITCH_CHECK(type_safe::get(outcome.failure_reason).find("skipped") != std::string::npos);
}

SNITCH_TEST_CASE("[wait_for_human_handler] unrecognised key selects first choice -- 3.2-U-010")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::text, .text = "Z"});
    WaitForHumanHandler h{qi};
    Context ctx;
    auto g    = make_approve_fix_graph();
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"[A] Approve"});
}

SNITCH_TEST_CASE("[wait_for_human_handler] AnswerKind::yes routes to Y-keyed edge -- 7.5-U-006")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::yes});
    WaitForHumanHandler h{qi};
    Context ctx;
    auto g    = make_single_edge_graph("gate", "yes", "[Y] Yes");
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"[Y] Yes"});
}

SNITCH_TEST_CASE("[wait_for_human_handler] AnswerKind::no routes to N-keyed edge -- 7.5-U-007")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::no});
    WaitForHumanHandler h{qi};
    Context ctx;
    Graph g;
    Edge e1;
    e1.from  = NodeId{"gate"};
    e1.to    = NodeId{"no_branch"};
    e1.label = EdgeLabel{"[N] No"};
    Edge e2;
    e2.from  = NodeId{"gate"};
    e2.to    = NodeId{"yes_branch"};
    e2.label = EdgeLabel{"[Y] Yes"};
    g.edges.push_back(e1);
    g.edges.push_back(e2);
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"[N] No"});
}

SNITCH_TEST_CASE("[wait_for_human_handler] non-printable accelerator in [X] pattern not matched -- 7.5-U-008")
{
    TempLogsDir logs;
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::text, .text = "\x01"});
    WaitForHumanHandler h{qi};
    Context ctx;
    Graph g;
    Edge e1;
    e1.from  = NodeId{"gate"};
    e1.to    = NodeId{"approve"};
    e1.label = EdgeLabel{"[A] Approve"};
    Edge e2;
    e2.from  = NodeId{"gate"};
    e2.to    = NodeId{"bad"};
    e2.label = EdgeLabel{"[\x01] bad"};
    g.edges.push_back(e1);
    g.edges.push_back(e2);
    auto gate = make_gate_node("gate");

    auto outcome = h.execute(gate, ctx, g, logs.logs_root());

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(outcome.preferred_label == EdgeLabel{"[A] Approve"});
}
