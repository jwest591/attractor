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

// Handler that returns a configurable outcome (including preferred_label and context_updates)
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

// Build a registry with a named configurable handler for a specific node type.
// The source node dispatches ConfigurableHandler; destination nodes use StartHandler (no-op).
HandlerRegistry make_registry(const std::string& source_type, Outcome source_outcome)
{
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{source_type},
                         std::make_unique<ConfigurableHandler>(std::move(source_outcome)));
    reg.set_default_handler(std::make_unique<StartHandler>());
    return reg;
}

}  // namespace

// -- DoD SS11.9: empty condition always true ------------------------------------

SNITCH_TEST_CASE("[condition_eval] empty condition: unconditional edge always selected")
{
    // Single edge from source node, no condition -- must be followed regardless of outcome
    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            src   [type="src", shape=box]
            done  [shape=Msquare]
            start -> src -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{make_registry("src", Outcome{})};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
}

// -- DoD SS11.9: outcome variable ----------------------------------------------

SNITCH_TEST_CASE("[condition_eval] outcome=success matches SUCCESS outcome -- 2.4-I-004")
{
    // Two edges: conditional (outcome=success -> cond_dest) and unconditional (uncond_dest).
    // Source returns SUCCESS -> conditional edge wins.
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
            start      [shape=Mdiamond]
            src        [type="src",        shape=box]
            cond_dest  [type="cond_dest",  shape=box]
            uncond_dest [type="uncond_dest", shape=box]
            done       [shape=Msquare]
            start -> src
            src -> cond_dest  [condition="outcome=success"]
            src -> uncond_dest
            cond_dest  -> done
            uncond_dest -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_cond->call_count == 1);
    SNITCH_CHECK(raw_uncond->call_count == 0);
}

SNITCH_TEST_CASE("[condition_eval] outcome=success does NOT match FAIL outcome")
{
    // Source returns FAIL -> condition="outcome=success" is false -> falls through.
    // No unconditional edge and outcome is fail -> FAIL propagated.
    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            src   [type="src", shape=box]
            dest  [shape=box]
            done  [shape=Msquare]
            start -> src
            src -> dest [condition="outcome=success"]
            dest -> done
        }
    )");
    TempLogsDir logs;
    Outcome fail_out = Outcome::fail(DiagnosticMessage{"test fail"});
    Engine engine{make_registry("src", std::move(fail_out))};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::fail);
}

SNITCH_TEST_CASE("[condition_eval] outcome=fail matches FAIL outcome")
{
    // Source returns FAIL -> condition="outcome=fail" is true -> follows conditional edge.
    auto rec = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw = rec.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"},
                         std::make_unique<ConfigurableHandler>(Outcome::fail(DiagnosticMessage{"test"})));
    reg.register_handler(HandlerTypeName{"dest"}, std::move(rec));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            src   [type="src",  shape=box]
            dest  [type="dest", shape=box]
            done  [shape=Msquare]
            start -> src
            src -> dest [condition="outcome=fail"]
            dest -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(raw->call_count == 1);
}

// -- DoD SS11.9: != operator ----------------------------------------------------

SNITCH_TEST_CASE("[condition_eval] outcome!=success matches FAIL outcome")
{
    auto rec = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw = rec.get();

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"},
                         std::make_unique<ConfigurableHandler>(Outcome::fail(DiagnosticMessage{"test"})));
    reg.register_handler(HandlerTypeName{"dest"}, std::move(rec));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            src   [type="src",  shape=box]
            dest  [type="dest", shape=box]
            done  [shape=Msquare]
            start -> src
            src -> dest [condition="outcome!=success"]
            dest -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw->call_count == 1);
}

// -- DoD SS11.9: && conjunction -------------------------------------------------

SNITCH_TEST_CASE("[condition_eval] && conjunction: both clauses true -> matches")
{
    auto rec = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw = rec.get();

    Outcome src_out;
    src_out.context_updates["flag"] = "yes";

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(std::move(src_out)));
    reg.register_handler(HandlerTypeName{"dest"}, std::move(rec));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            src   [type="src",  shape=box]
            dest  [type="dest", shape=box]
            done  [shape=Msquare]
            start -> src
            src -> dest [condition="outcome=success && context.flag=yes"]
            dest -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw->call_count == 1);
}

SNITCH_TEST_CASE("[condition_eval] && conjunction: one clause false -> no match")
{
    auto rec = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw = rec.get();

    // context.flag is not set -> context.flag=yes is false -> conjunction fails
    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(Outcome{}));
    reg.register_handler(HandlerTypeName{"dest"}, std::move(rec));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            src   [type="src",  shape=box]
            dest  [type="dest", shape=box]
            done  [shape=Msquare]
            start -> src
            src -> dest [condition="outcome=success && context.flag=yes"]
            src -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw->call_count == 0);
    SNITCH_CHECK(outcome.status == StageStatus::success);
}

// -- DoD SS11.9: preferred_label variable ---------------------------------------

SNITCH_TEST_CASE("[condition_eval] preferred_label variable resolves from outcome")
{
    // Source returns preferred_label="Fix". Conditional edge condition="preferred_label=Fix"
    // matches via Step 1 and is selected over unconditional edge.
    auto rec_cond = std::make_unique<RecordingHandler>();
    auto rec_uncond = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_cond = rec_cond.get();
    const RecordingHandler* raw_uncond = rec_uncond.get();

    Outcome src_out;
    src_out.preferred_label = EdgeLabel{"Fix"};

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(std::move(src_out)));
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
            src -> cond_dest   [condition="preferred_label=Fix"]
            src -> uncond_dest
            cond_dest  -> done
            uncond_dest -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw_cond->call_count == 1);
    SNITCH_CHECK(raw_uncond->call_count == 0);
}

// -- DoD SS11.9: context.* keys -------------------------------------------------

SNITCH_TEST_CASE("[condition_eval] context.key=value matches when key is in context_updates")
{
    auto rec = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw = rec.get();

    Outcome src_out;
    src_out.context_updates["tests_passed"] = "true";

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"src"}, std::make_unique<ConfigurableHandler>(std::move(src_out)));
    reg.register_handler(HandlerTypeName{"dest"}, std::move(rec));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start [shape=Mdiamond]
            src   [type="src",  shape=box]
            dest  [type="dest", shape=box]
            done  [shape=Msquare]
            start -> src
            src -> dest [condition="context.tests_passed=true"]
            dest -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    (void)engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(raw->call_count == 1);
}

SNITCH_TEST_CASE("[condition_eval] context.key=value does NOT match when key is absent (empty string)")
{
    // Missing key resolves to empty string -> "context.absent_key=value" is false.
    // Source has a conditional edge (won't be taken) and an unconditional edge (will be taken).
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
            src -> cond_dest   [condition="context.absent_key=value"]
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

// -- Cross-node context accumulation ------------------------------------------

SNITCH_TEST_CASE("[condition_eval] context set by node A is visible to node C's edge condition")
{
    // node_a sets context.phase=done; node_b runs unconditionally with no context updates;
    // node_c's incoming edge has condition="context.phase=done".
    // Verifies that context_snapshot accumulates across multiple nodes in the pipeline.
    auto rec_c = std::make_unique<RecordingHandler>();
    const RecordingHandler* raw_c = rec_c.get();

    Outcome a_out;
    a_out.context_updates["phase"] = "done";

    HandlerRegistry reg;
    reg.register_handler(HandlerTypeName{"start"}, std::make_unique<StartHandler>());
    reg.register_handler(HandlerTypeName{"exit"}, std::make_unique<ExitHandler>());
    reg.register_handler(HandlerTypeName{"node_a"}, std::make_unique<ConfigurableHandler>(std::move(a_out)));
    reg.register_handler(HandlerTypeName{"node_b"}, std::make_unique<ConfigurableHandler>(Outcome{}));
    reg.register_handler(HandlerTypeName{"node_c"}, std::move(rec_c));
    reg.set_default_handler(std::make_unique<StartHandler>());

    auto graph = parse_ok(R"(
        digraph {
            start  [shape=Mdiamond]
            node_a [type="node_a", shape=box]
            node_b [type="node_b", shape=box]
            node_c [type="node_c", shape=box]
            done   [shape=Msquare]
            start -> node_a
            node_a -> node_b
            node_b -> node_c [condition="context.phase=done"]
            node_c -> done
        }
    )");
    TempLogsDir logs;
    Engine engine{std::move(reg)};

    auto outcome = engine.run(graph, RunConfig{.logs_root = logs.logs_root()});

    SNITCH_CHECK(outcome.status == StageStatus::success);
    SNITCH_CHECK(raw_c->call_count == 1);
}
