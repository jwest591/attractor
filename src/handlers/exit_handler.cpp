#include <attractor/handlers/exit_handler.hpp>

namespace attractor {

auto ExitHandler::execute(const Node& /*node*/, Context& /*ctx*/, const Graph& /*graph*/,
                          const LogsRoot& /*logs_root*/) const -> Outcome
{
    return Outcome{};
}

}  // namespace attractor
