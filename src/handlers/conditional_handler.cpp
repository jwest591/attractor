#include <attractor/handlers/conditional_handler.hpp>

#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

namespace attractor {

auto ConditionalHandler::execute(const Node& node, Context& /*ctx*/, const Graph& /*graph*/,
                                 const RunConfig& /*run_config*/) const -> Outcome
{
    return Outcome{
        .notes = HandlerNote{"Conditional node evaluated: " + type_safe::get(node.id)},
    };
}

}  // namespace attractor
