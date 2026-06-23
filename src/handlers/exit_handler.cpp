#include <attractor/handlers/exit_handler.hpp>

namespace attractor {

auto ExitHandler::execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                          const RunConfig& /*run_config*/) const -> Outcome
{
    return Outcome{};
}

}  // namespace attractor
