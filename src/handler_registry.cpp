#include <attractor/handler_registry.hpp>

#include <cassert>
#include <utility>

namespace attractor {

void HandlerRegistry::register_handler(const HandlerTypeName& type, std::unique_ptr<Handler> handler)
{
    m_handlers[type] = std::move(handler);
}

void HandlerRegistry::set_default_handler(std::unique_ptr<Handler> handler) { m_default_handler = std::move(handler); }

auto HandlerRegistry::shape_to_handler_type(NodeShape shape) -> HandlerTypeName
{
    switch (shape) {
    case NodeShape::mdiamond:      return HandlerTypeName{"start"};
    case NodeShape::msquare:       return HandlerTypeName{"exit"};
    case NodeShape::box:           return HandlerTypeName{"codergen"};
    case NodeShape::hexagon:       return HandlerTypeName{"wait.human"};
    case NodeShape::diamond:       return HandlerTypeName{"conditional"};
    case NodeShape::component:     return HandlerTypeName{"parallel"};
    case NodeShape::triple_octagon:return HandlerTypeName{"parallel.fan_in"};
    case NodeShape::parallelogram: return HandlerTypeName{"tool"};
    case NodeShape::house:         return HandlerTypeName{"stack.manager_loop"};
    }
    std::unreachable();
}

auto HandlerRegistry::resolve(const Node& node) const -> const Handler&
{
    // Step 1: explicit type attribute
    if (!type_safe::get(node.node_type).empty()) {
        if (auto it = m_handlers.find(node.node_type); it != m_handlers.end()) {
            return *it->second;
        }
    }

    // Step 2: shape-based resolution
    {
        const auto mapped = shape_to_handler_type(node.shape);
        if (auto it = m_handlers.find(mapped); it != m_handlers.end()) {
            return *it->second;
        }
    }

    // Step 3: default handler (programming error if never set)
    assert(m_default_handler != nullptr);
    return *m_default_handler;
}

}  // namespace attractor
