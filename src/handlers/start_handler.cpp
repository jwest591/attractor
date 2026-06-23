#include <attractor/handlers/start_handler.hpp>

namespace attractor {

auto StartHandler::execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                           const RunConfig& /*run_config*/) const -> Outcome
{
    return Outcome{};
}

}  // namespace attractor
