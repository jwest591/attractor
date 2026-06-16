#ifndef ATTRACTOR_CLI_BACKENDS_HANDOFF_AWARE_BACKEND_HPP
#define ATTRACTOR_CLI_BACKENDS_HANDOFF_AWARE_BACKEND_HPP

#include <attractor/handler.hpp>
#include <expected>
#include <memory>

namespace attractor {

class HandoffAwareBackend final : public CodergenBackend {
  public:
    static constexpr int k_default_max_handoffs = 3;

    explicit HandoffAwareBackend(std::unique_ptr<CodergenBackend> inner,
                                 int max_handoffs = k_default_max_handoffs);

    [[nodiscard]] auto run(const Node& node, const PromptText& prompt,
                           Context& ctx) const -> std::expected<LlmResponse, Outcome> override;

  private:
    std::unique_ptr<CodergenBackend> m_inner;
    int m_max_handoffs;
};

} // namespace attractor

#endif // ATTRACTOR_CLI_BACKENDS_HANDOFF_AWARE_BACKEND_HPP
