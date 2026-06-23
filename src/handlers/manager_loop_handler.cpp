#include <attractor/handlers/manager_loop_handler.hpp>

#include <attractor/condition_eval.hpp>
#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace attractor {

auto ManagerLoopHandler::execute(const Node& node, Context& ctx, const Graph& /*graph*/,
                                 const RunConfig& /*run_config*/) const -> Outcome
{
    const int max_cycles = node.manager_max_cycles;
    const ConditionExpr& stop_cond = node.manager_stop_condition;

    for (int cycle = 1; cycle <= max_cycles; ++cycle) {
        // Write current cycle number to context so stop conditions can reference it.
        // Key "cycle" is a flat context key -- conditions use "context.cycle=N".
        (void)ctx.set(ContextKey{"cycle"}, std::to_string(cycle));

        if (!stop_cond.empty()) {
            const nlohmann::json snapshot = ctx.snapshot();
            // Outcome{} is intentional: manager_loop stop conditions are context-based only.
            // outcome= and preferred_label= clauses silently never match here.
            if (eval_condition(stop_cond, Outcome{}, snapshot)) {
                return Outcome{
                    .notes = HandlerNote{"Stop condition satisfied on cycle " + std::to_string(cycle)},
                };
            }
        }
    }

    return Outcome::fail(DiagnosticMessage{"Max cycles exceeded"});
}

}  // namespace attractor
