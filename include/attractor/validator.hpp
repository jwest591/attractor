#ifndef ATTRACTOR_VALIDATOR_HPP
#define ATTRACTOR_VALIDATOR_HPP

#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace attractor {

struct Diagnostic {
    RuleId rule_id;
    Severity severity;
    std::optional<NodeId> node_id = std::nullopt;
    std::optional<NodeId> to_node_id = std::nullopt;
    DiagnosticMessage message;
    SuggestedFix suggested_fix = {};
};

class ValidationError : public std::runtime_error {
  public:
    explicit ValidationError(std::vector<Diagnostic> errors);
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept;

  private:
    std::vector<Diagnostic> m_diagnostics;
};

class LintRule {
  public:
    virtual ~LintRule() = default;
    [[nodiscard]] virtual auto apply(const Graph& graph) const -> std::vector<Diagnostic> = 0;
};

struct ValidationConfig {
    std::vector<HandlerTypeName> known_types;
};

[[nodiscard]] auto validate(const Graph& graph, const ValidationConfig& config = {},
                            std::vector<const LintRule*> extra_rules = {}) -> std::vector<Diagnostic>;

void validate_or_raise(const Graph& graph, const ValidationConfig& config = {},
                       std::vector<const LintRule*> extra_rules = {});

}  // namespace attractor

#endif  // ATTRACTOR_VALIDATOR_HPP
