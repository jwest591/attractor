#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/engine.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/handler_registry.hpp>
#include <attractor/handlers/exit_handler.hpp>
#include <attractor/handlers/start_handler.hpp>
#include <attractor/types.hpp>
#include <memory>
#include <string>

using namespace attractor;
using namespace attractor::test;

namespace {

class RecordingHandler final : public Handler {
  public:
    mutable int call_count{0};

    [[nodiscard]] auto execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                               const RunConfig& /*run_config*/) const -> Outcome override
    {
        ++call_count;
        return Outcome{};
    }
};

class ConfigurableHandler final : public Handler {
  public:
    explicit ConfigurableHandler(Outcome out) : m_outcome{std::move(out)} {}

    [[nodiscard]] auto execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                               const RunConfig& /*run_config*/) const -> Outcome override
    {
        return m_outcome;
    }

  private:
    Outcome m_outcome;
};

}  // namespace

// -- AC1: Step 1 -- condition match wins over unconditional --------------------

SNITCH_TEST_CASE("[edge_selection] Step 1: condition match wins over high-weight unconditional -- 2.4-I-001")
{
    // Source returns SUCCESS.
    // Edge A [condition="outcome=success", weight=0] vs Edge B [unconditional, weight=10]
    // Step 1 (condition match) must win over Step 4 (weight).
    auto rec_a = std::make_unique<RecordingHandler>();
    auto rec_b = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_a = rec_a.get();
    const RecordingHandler* raw_b = rec_b.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(Outcome{}));
    reg.register_handler(HandlerTypeName{"node_a"}, std::move(rec_a));
    reg.register_handler(HandlerTypeName{"node_b"}, std::move(rec_b));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start  [shape=Mdiamond]
            src    [type="src",    shape=box]
            node_a [type="node_a", shape=box]
            node_b [type="node_b", shape=box]
            done   [shape=Msquare]
            start -> src
            src -> node_a [condition="outcome=success", weight=0]
            src -> node_b [weight=10]
            node_a -> done
            node_b -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_a->call_count == 1);
    SNITCH_CHECK(raw_b->call_count == 0);
}

// -- AC2: Step 2 -- preferred_label match -------------------------------------

SNITCH_TEST_CASE("[edge_selection] Step 2: preferred_label match beats higher-weight unconditional -- 2.4-I-002")
{
    // Source returns preferred_label="Fix".
    // Two unconditional edges: "Fix" (weight=0) and "Continue" (weight=10).
    // Step 2 must route to "Fix" edge.
    auto rec_fix = std::make_unique<RecordingHandler>();
    auto rec_cont = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_fix = rec_fix.get();
    const RecordingHandler* raw_cont = rec_cont.get();

    Outcome src_out;
    src_out.preferred_label = EdgeLabel{"Fix"};

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(std::move(src_out)));
    reg.register_handler(HandlerTypeName{"fix_node"}, std::move(rec_fix));
    reg.register_handler(HandlerTypeName{"cont_node"}, std::move(rec_cont));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start     [shape=Mdiamond]
            src       [type="src",       shape=box]
            fix_node  [type="fix_node",  shape=box]
            cont_node [type="cont_node", shape=box]
            done      [shape=Msquare]
            start -> src
            src -> fix_node  [label="Fix",      weight=0]
            src -> cont_node [label="Continue",  weight=10]
            fix_node  -> done
            cont_node -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_fix->call_count == 1);
    SNITCH_CHECK(raw_cont->call_count == 0);
}

// -- AC4: Step 5 -- lexical tiebreak ------------------------------------------

SNITCH_TEST_CASE("[edge_selection] Step 5: lexical tiebreak selects alpha before beta")
{
    // Two unconditional edges to "alpha_node" and "beta_node" with equal weight=0.
    // No preferred_label, no conditions. "alpha_node" < "beta_node" lexically.
    auto rec_alpha = std::make_unique<RecordingHandler>();
    auto rec_beta = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_alpha = rec_alpha.get();
    const RecordingHandler* raw_beta = rec_beta.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(Outcome{}));
    reg.register_handler(HandlerTypeName{"alpha_node"}, std::move(rec_alpha));
    reg.register_handler(HandlerTypeName{"beta_node"}, std::move(rec_beta));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start      [shape=Mdiamond]
            src        [type="src",        shape=box]
            alpha_node [type="alpha_node", shape=box]
            beta_node  [type="beta_node",  shape=box]
            done       [shape=Msquare]
            start -> src
            src -> beta_node  [weight=0]
            src -> alpha_node [weight=0]
            alpha_node -> done
            beta_node  -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_alpha->call_count == 1);
    SNITCH_CHECK(raw_beta->call_count == 0);
}

// -- DoD SS11.12: conditional branching success/fail paths -- 2.4-I-003 --------

SNITCH_TEST_CASE("[edge_selection] conditional branching: success path -- 2.4-I-003")
{
    // Node with condition="outcome=success" goes to success_dest;
    // condition="outcome=fail" goes to fail_dest.
    // Handler returns SUCCESS -> success_dest is reached.
    auto rec_s = std::make_unique<RecordingHandler>();
    auto rec_f = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_s = rec_s.get();
    const RecordingHandler* raw_f = rec_f.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(Outcome{}));
    reg.register_handler(HandlerTypeName{"success_dest"}, std::move(rec_s));
    reg.register_handler(HandlerTypeName{"fail_dest"}, std::move(rec_f));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start        [shape=Mdiamond]
            src          [type="src",         shape=box]
            success_dest [type="success_dest", shape=box]
            fail_dest    [type="fail_dest",    shape=box]
            done         [shape=Msquare]
            start -> src
            src -> success_dest [condition="outcome=success"]
            src -> fail_dest    [condition="outcome=fail"]
            success_dest -> done
            fail_dest    -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(raw_s->call_count == 1);
    SNITCH_CHECK(raw_f->call_count == 0);
}

SNITCH_TEST_CASE("[edge_selection] conditional branching: fail path -- 2.4-I-005")
{
    // Same graph as above; handler returns FAIL -> fail_dest is reached.
    auto rec_s = std::make_unique<RecordingHandler>();
    auto rec_f = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_s = rec_s.get();
    const RecordingHandler* raw_f = rec_f.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"},
                         std::make_unique<ConfigurableHandler>(Outcome::fail(DiagnosticMessage{"test"})));
    reg.register_handler(HandlerTypeName{"success_dest"}, std::move(rec_s));
    reg.register_handler(HandlerTypeName{"fail_dest"}, std::move(rec_f));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start        [shape=Mdiamond]
            src          [type="src",         shape=box]
            success_dest [type="success_dest", shape=box]
            fail_dest    [type="fail_dest",    shape=box]
            done         [shape=Msquare]
            start -> src
            src -> success_dest [condition="outcome=success"]
            src -> fail_dest    [condition="outcome=fail"]
            success_dest -> done
            fail_dest    -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);  // reached done via fail_dest
    SNITCH_CHECK(raw_s->call_count == 0);
    SNITCH_CHECK(raw_f->call_count == 1);
}

// -- AC3 / Step 3 -- suggested_next_ids match ---------------------------------

SNITCH_TEST_CASE("[edge_selection] Step 3: suggested_next_ids routes to named node over higher-weight edge")
{
    // Source returns suggested_next_ids=["target_node"].
    // Two unconditional edges: target_node (weight=0) and other_node (weight=10).
    // Step 3 must route to target_node despite lower weight.
    auto rec_target = std::make_unique<RecordingHandler>();
    auto rec_other = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_target = rec_target.get();
    const RecordingHandler* raw_other = rec_other.get();

    Outcome src_out;
    src_out.suggested_next_ids.emplace_back(NodeId{"target_node"});

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(std::move(src_out)));
    reg.register_handler(HandlerTypeName{"target_node"}, std::move(rec_target));
    reg.register_handler(HandlerTypeName{"other_node"}, std::move(rec_other));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start       [shape=Mdiamond]
            src         [type="src",         shape=box]
            target_node [type="target_node", shape=box]
            other_node  [type="other_node",  shape=box]
            done        [shape=Msquare]
            start -> src
            src -> target_node [weight=0]
            src -> other_node  [weight=10]
            target_node -> done
            other_node  -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_target->call_count == 1);
    SNITCH_CHECK(raw_other->call_count == 0);
}

// -- False condition falls through to unconditional ---------------------------

SNITCH_TEST_CASE("[edge_selection] false condition falls through to unconditional edge")
{
    // condition="outcome=fail" is false (outcome=SUCCESS) -> falls to unconditional edge.
    auto rec_cond = std::make_unique<RecordingHandler>();
    auto rec_uncond = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_cond = rec_cond.get();
    const RecordingHandler* raw_uncond = rec_uncond.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(Outcome{}));
    reg.register_handler(HandlerTypeName{"cond_dest"}, std::move(rec_cond));
    reg.register_handler(HandlerTypeName{"uncond_dest"}, std::move(rec_uncond));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start       [shape=Mdiamond]
            src         [type="src",        shape=box]
            cond_dest   [type="cond_dest",  shape=box]
            uncond_dest [type="uncond_dest", shape=box]
            done        [shape=Msquare]
            start -> src
            src -> cond_dest   [condition="outcome=fail"]
            src -> uncond_dest
            cond_dest  -> done
            uncond_dest -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_cond->call_count == 0);
    SNITCH_CHECK(raw_uncond->call_count == 1);
}

// -- Label normalization -------------------------------------------------------

SNITCH_TEST_CASE("[edge_selection] Step 2: normalized label [A] Approve matches preferred_label Approve")
{
    // preferred_label="Approve"; edge label is "[A] Approve".
    // normalize_label("[A] Approve") -> "approve"; normalize_label("Approve") -> "approve"
    // -> match.
    auto rec_labeled = std::make_unique<RecordingHandler>();
    auto rec_unlabeled = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_labeled = rec_labeled.get();
    const RecordingHandler* raw_unlabeled = rec_unlabeled.get();

    Outcome src_out;
    src_out.preferred_label = EdgeLabel{"Approve"};

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(std::move(src_out)));
    reg.register_handler(HandlerTypeName{"labeled"}, std::move(rec_labeled));
    reg.register_handler(HandlerTypeName{"unlabeled"}, std::move(rec_unlabeled));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start     [shape=Mdiamond]
            src       [type="src",      shape=box]
            labeled   [type="labeled",   shape=box]
            unlabeled [type="unlabeled", shape=box]
            done      [shape=Msquare]
            start -> src
            src -> labeled   [label="[A] Approve", weight=0]
            src -> unlabeled [weight=5]
            labeled   -> done
            unlabeled -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_labeled->call_count == 1);
    SNITCH_CHECK(raw_unlabeled->call_count == 0);
}

SNITCH_TEST_CASE("[edge_selection] Step 2: normalized label A) Approve matches preferred_label Approve")
{
    // preferred_label="Approve"; edge label is "A) Approve".
    // normalize_label("A) Approve") -> "approve"; normalize_label("Approve") -> "approve" -> match.
    auto rec_labeled = std::make_unique<RecordingHandler>();
    auto rec_unlabeled = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_labeled = rec_labeled.get();
    const RecordingHandler* raw_unlabeled = rec_unlabeled.get();

    Outcome src_out;
    src_out.preferred_label = EdgeLabel{"Approve"};

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(std::move(src_out)));
    reg.register_handler(HandlerTypeName{"labeled"}, std::move(rec_labeled));
    reg.register_handler(HandlerTypeName{"unlabeled"}, std::move(rec_unlabeled));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start     [shape=Mdiamond]
            src       [type="src",      shape=box]
            labeled   [type="labeled",   shape=box]
            unlabeled [type="unlabeled", shape=box]
            done      [shape=Msquare]
            start -> src
            src -> labeled   [label="A) Approve", weight=0]
            src -> unlabeled [weight=5]
            labeled   -> done
            unlabeled -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_labeled->call_count == 1);
    SNITCH_CHECK(raw_unlabeled->call_count == 0);
}

SNITCH_TEST_CASE("[edge_selection] Step 2: normalized label A - Approve matches preferred_label Approve")
{
    // preferred_label="Approve"; edge label is "A - Approve".
    // normalize_label("A - Approve") -> "approve"; normalize_label("Approve") -> "approve" -> match.
    auto rec_labeled = std::make_unique<RecordingHandler>();
    auto rec_unlabeled = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_labeled = rec_labeled.get();
    const RecordingHandler* raw_unlabeled = rec_unlabeled.get();

    Outcome src_out;
    src_out.preferred_label = EdgeLabel{"Approve"};

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(std::move(src_out)));
    reg.register_handler(HandlerTypeName{"labeled"}, std::move(rec_labeled));
    reg.register_handler(HandlerTypeName{"unlabeled"}, std::move(rec_unlabeled));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start     [shape=Mdiamond]
            src       [type="src",      shape=box]
            labeled   [type="labeled",   shape=box]
            unlabeled [type="unlabeled", shape=box]
            done      [shape=Msquare]
            start -> src
            src -> labeled   [label="A - Approve", weight=0]
            src -> unlabeled [weight=5]
            labeled   -> done
            unlabeled -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_labeled->call_count == 1);
    SNITCH_CHECK(raw_unlabeled->call_count == 0);
}
