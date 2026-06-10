#ifndef ATTRACTOR_BACKENDS_NOOP_BACKEND_HPP
#define ATTRACTOR_BACKENDS_NOOP_BACKEND_HPP

#include <attractor/handler.hpp>

namespace attractor {

class NoOpBackend final : public CodergenBackend {
  public:
    [[nodiscard]] auto run(const Node& node, const PromptText& prompt,
                           Context& ctx) const -> std::expected<LlmResponse, Outcome> override;
};

}  // namespace attractor

#endif  // ATTRACTOR_BACKENDS_NOOP_BACKEND_HPP
