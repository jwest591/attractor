#ifndef ATTRACTOR_CONDITION_EVAL_HPP
#define ATTRACTOR_CONDITION_EVAL_HPP

#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <nlohmann/json.hpp>

namespace attractor {

// Returns true if condition is empty or all &&-joined clauses evaluate true.
// Defined in src/engine.cpp -- including this header without engine.cpp in the build
// will produce a linker error on eval_condition.
// Supports: outcome=X, preferred_label=X, context.key=X, != operator.
bool eval_condition(const ConditionExpr& condition, const Outcome& outcome,
                    const nlohmann::json& context_snapshot);

}  // namespace attractor

#endif  // ATTRACTOR_CONDITION_EVAL_HPP
