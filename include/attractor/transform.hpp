#ifndef ATTRACTOR_TRANSFORM_HPP
#define ATTRACTOR_TRANSFORM_HPP

#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <vector>

namespace attractor {

class Transform {
  public:
    virtual ~Transform() = default;
    [[nodiscard]] virtual auto apply(const Graph& graph) const -> Graph = 0;
};

class VariableExpansionTransform final : public Transform {
  public:
    [[nodiscard]] auto apply(const Graph& graph) const -> Graph override;
};

class StylesheetTransform final : public Transform {
  public:
    [[nodiscard]] auto apply(const Graph& graph) const -> Graph override;
};

[[nodiscard]] auto apply_transforms(const Graph& graph,
                                    const std::vector<const Transform*>& custom_transforms = {}) -> Graph;

}  // namespace attractor

#endif  // ATTRACTOR_TRANSFORM_HPP
