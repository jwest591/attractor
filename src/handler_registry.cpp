#include <attractor/handler_registry.hpp>

#include <cassert>
#include <optional>
#include <string>
#include <type_safe/strong_typedef.hpp>
#include <unordered_map>

namespace attractor {

void HandlerRegistry::register_handler(const HandlerTypeName& type, std::unique_ptr<Handler> handler)
{
    m_handlers[type] = std::move(handler);
}

void HandlerRegistry::set_default_handler(std::unique_ptr<Handler> handler) { m_default_handler = std::move(handler); }

auto HandlerRegistry::shape_to_handler_type(const NodeShape& shape) -> std::optional<HandlerTypeName>
{
    static const std::unordered_map<std::string, std::string> k_shape_map{
        {"Mdiamond",      "start"             },
        {"Msquare",       "exit"              },
        {"box",           "codergen"          },
        {"hexagon",       "wait.human"        },
        {"diamond",       "conditional"       },
        {"component",     "parallel"          },
        {"tripleoctagon", "parallel.fan_in"   },
        {"parallelogram", "tool"              },
        {"house",         "stack.manager_loop"},
    };
    auto it = k_shape_map.find(type_safe::get(shape));
    if (it == k_shape_map.end()) {
        return std::nullopt;
    }
    return HandlerTypeName{it->second};
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
    if (auto mapped = shape_to_handler_type(node.shape)) {
        if (auto it = m_handlers.find(*mapped); it != m_handlers.end()) {
            return *it->second;
        }
    }

    // Step 3: default handler (programming error if never set)
    assert(m_default_handler != nullptr);
    return *m_default_handler;
}

}  // namespace attractor
