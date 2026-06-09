#ifndef ATTRACTOR_HANDLER_REGISTRY_HPP
#define ATTRACTOR_HANDLER_REGISTRY_HPP

#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <memory>
#include <optional>
#include <unordered_map>

namespace attractor {

class HandlerRegistry {
  public:
    void register_handler(const HandlerTypeName& type, std::unique_ptr<Handler> handler);
    void set_default_handler(std::unique_ptr<Handler> handler);

    // Returns a non-owning reference valid for the lifetime of this registry.
    // Precondition: set_default_handler() must have been called before resolving
    // any node that does not match a registered type or shape.
    [[nodiscard]] auto resolve(const Node& node) const -> const Handler&;

    [[nodiscard]] static auto shape_to_handler_type(const NodeShape& shape) -> std::optional<HandlerTypeName>;

  private:
    std::unordered_map<HandlerTypeName, std::unique_ptr<Handler>> m_handlers;
    std::unique_ptr<Handler> m_default_handler;
};

}  // namespace attractor

#endif  // ATTRACTOR_HANDLER_REGISTRY_HPP
