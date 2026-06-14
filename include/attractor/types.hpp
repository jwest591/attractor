#ifndef ATTRACTOR_TYPES_HPP
#define ATTRACTOR_TYPES_HPP

#include <chrono>
#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <type_safe/boolean.hpp>
#include <type_safe/constrained_type.hpp>
#include <type_safe/strong_typedef.hpp>

namespace attractor {

namespace ts = type_safe;

// -- String strong typedefs (AC1) ----------------------------------------------

struct NodeId
    : ts::strong_typedef<NodeId, std::string>
    , ts::strong_typedef_op::equality_comparison<NodeId>
    , ts::strong_typedef_op::relational_comparison<NodeId> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct EdgeLabel
    : ts::strong_typedef<EdgeLabel, std::string>
    , ts::strong_typedef_op::equality_comparison<EdgeLabel>
    , ts::strong_typedef_op::relational_comparison<EdgeLabel> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct ArtifactId
    : ts::strong_typedef<ArtifactId, std::string>
    , ts::strong_typedef_op::equality_comparison<ArtifactId>
    , ts::strong_typedef_op::relational_comparison<ArtifactId> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct HandlerTypeName
    : ts::strong_typedef<HandlerTypeName, std::string>
    , ts::strong_typedef_op::equality_comparison<HandlerTypeName>
    , ts::strong_typedef_op::relational_comparison<HandlerTypeName> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct ContextKey
    : ts::strong_typedef<ContextKey, std::string>
    , ts::strong_typedef_op::equality_comparison<ContextKey>
    , ts::strong_typedef_op::relational_comparison<ContextKey> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct ThreadId
    : ts::strong_typedef<ThreadId, std::string>
    , ts::strong_typedef_op::equality_comparison<ThreadId>
    , ts::strong_typedef_op::relational_comparison<ThreadId> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct PromptText
    : ts::strong_typedef<PromptText, std::string>
    , ts::strong_typedef_op::equality_comparison<PromptText>
    , ts::strong_typedef_op::relational_comparison<PromptText> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct GoalText
    : ts::strong_typedef<GoalText, std::string>
    , ts::strong_typedef_op::equality_comparison<GoalText>
    , ts::strong_typedef_op::relational_comparison<GoalText> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct LogsRoot
    : ts::strong_typedef<LogsRoot, std::string>
    , ts::strong_typedef_op::equality_comparison<LogsRoot>
    , ts::strong_typedef_op::relational_comparison<LogsRoot> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct NodeLabel
    : ts::strong_typedef<NodeLabel, std::string>
    , ts::strong_typedef_op::equality_comparison<NodeLabel>
    , ts::strong_typedef_op::relational_comparison<NodeLabel> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

enum class NodeShape {
    mdiamond,
    msquare,
    box,
    hexagon,
    diamond,
    component,
    triple_octagon,
    parallelogram,
    house,
};

struct CssClass
    : ts::strong_typedef<CssClass, std::string>
    , ts::strong_typedef_op::equality_comparison<CssClass>
    , ts::strong_typedef_op::relational_comparison<CssClass> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct LlmModel
    : ts::strong_typedef<LlmModel, std::string>
    , ts::strong_typedef_op::equality_comparison<LlmModel>
    , ts::strong_typedef_op::relational_comparison<LlmModel> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct LlmProvider
    : ts::strong_typedef<LlmProvider, std::string>
    , ts::strong_typedef_op::equality_comparison<LlmProvider>
    , ts::strong_typedef_op::relational_comparison<LlmProvider> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct GraphId
    : ts::strong_typedef<GraphId, std::string>
    , ts::strong_typedef_op::equality_comparison<GraphId>
    , ts::strong_typedef_op::relational_comparison<GraphId> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct GraphLabel
    : ts::strong_typedef<GraphLabel, std::string>
    , ts::strong_typedef_op::equality_comparison<GraphLabel>
    , ts::strong_typedef_op::relational_comparison<GraphLabel> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct StylesheetId
    : ts::strong_typedef<StylesheetId, std::string>
    , ts::strong_typedef_op::equality_comparison<StylesheetId>
    , ts::strong_typedef_op::relational_comparison<StylesheetId> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct DotfilePath
    : ts::strong_typedef<DotfilePath, std::string>
    , ts::strong_typedef_op::equality_comparison<DotfilePath>
    , ts::strong_typedef_op::relational_comparison<DotfilePath> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct WorkDir
    : ts::strong_typedef<WorkDir, std::string>
    , ts::strong_typedef_op::equality_comparison<WorkDir>
    , ts::strong_typedef_op::relational_comparison<WorkDir> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct ShellCommand
    : ts::strong_typedef<ShellCommand, std::string>
    , ts::strong_typedef_op::equality_comparison<ShellCommand>
    , ts::strong_typedef_op::relational_comparison<ShellCommand> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct ConditionExpr
    : ts::strong_typedef<ConditionExpr, std::string>
    , ts::strong_typedef_op::equality_comparison<ConditionExpr>
    , ts::strong_typedef_op::relational_comparison<ConditionExpr> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct RuleId
    : ts::strong_typedef<RuleId, std::string>
    , ts::strong_typedef_op::equality_comparison<RuleId>
    , ts::strong_typedef_op::relational_comparison<RuleId> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct DiagnosticMessage
    : ts::strong_typedef<DiagnosticMessage, std::string>
    , ts::strong_typedef_op::equality_comparison<DiagnosticMessage>
    , ts::strong_typedef_op::relational_comparison<DiagnosticMessage> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct SuggestedFix
    : ts::strong_typedef<SuggestedFix, std::string>
    , ts::strong_typedef_op::equality_comparison<SuggestedFix>
    , ts::strong_typedef_op::relational_comparison<SuggestedFix> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct HandlerNote
    : ts::strong_typedef<HandlerNote, std::string>
    , ts::strong_typedef_op::equality_comparison<HandlerNote>
    , ts::strong_typedef_op::relational_comparison<HandlerNote> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

struct LlmResponse
    : ts::strong_typedef<LlmResponse, std::string>
    , ts::strong_typedef_op::equality_comparison<LlmResponse>
    , ts::strong_typedef_op::relational_comparison<LlmResponse> {
    using strong_typedef::strong_typedef;

    bool empty() const noexcept { return static_cast<std::string>(*this).empty(); }
};

// -- Constrained types (AC2) --------------------------------------------------─
// Each constrained type uses its own constraint struct so the four `using`
// aliases produce distinct instantiations of ts::constrained_type.

struct max_retries_constraint {
    constexpr bool operator()(int v) const noexcept { return v >= 0; }
};

using MaxRetries = ts::constrained_type<int, max_retries_constraint, ts::assertion_verifier>;

struct weight_constraint {
    constexpr bool operator()(int v) const noexcept { return v >= 0; }
};

using Weight = ts::constrained_type<int, weight_constraint, ts::assertion_verifier>;

struct port_constraint {
    constexpr bool operator()(int v) const noexcept { return v >= 1 && v <= 65535; }
};

using Port = ts::constrained_type<int, port_constraint, ts::assertion_verifier>;

struct positive_duration_constraint {
    constexpr bool operator()(std::chrono::milliseconds v) const noexcept { return v.count() > 0; }
};

using TimeoutDuration =
    ts::constrained_type<std::chrono::milliseconds, positive_duration_constraint, ts::assertion_verifier>;

struct max_parallel_constraint {
    constexpr bool operator()(int v) const noexcept { return v >= 1; }
};

using MaxParallel = ts::constrained_type<int, max_parallel_constraint, ts::assertion_verifier>;

// -- Enum classes --------------------------------------------------------------

enum class StageStatus { success, partial_success, fail, retry, skipped };

enum class Severity { error, warning, info };

enum class QuestionType { yes_no, multiple_choice, freeform, confirmation };

enum class AnswerKind { yes, no, text, skipped, timeout };

enum class FidelityMode { full, truncate, compact, summary_low, summary_medium, summary_high };

enum class ReasoningEffort { low, medium, high };

enum class JoinPolicy { wait_all, first_success };

// -- JSON serialization declarations (defined in src/types.cpp) --------------─

void to_json(nlohmann::json& j, const NodeId& v);
void from_json(const nlohmann::json& j, NodeId& v);

void to_json(nlohmann::json& j, const EdgeLabel& v);
void from_json(const nlohmann::json& j, EdgeLabel& v);

void to_json(nlohmann::json& j, const ArtifactId& v);
void from_json(const nlohmann::json& j, ArtifactId& v);

void to_json(nlohmann::json& j, const HandlerTypeName& v);
void from_json(const nlohmann::json& j, HandlerTypeName& v);

void to_json(nlohmann::json& j, const ContextKey& v);
void from_json(const nlohmann::json& j, ContextKey& v);

void to_json(nlohmann::json& j, const ThreadId& v);
void from_json(const nlohmann::json& j, ThreadId& v);

void to_json(nlohmann::json& j, const PromptText& v);
void from_json(const nlohmann::json& j, PromptText& v);

void to_json(nlohmann::json& j, const GoalText& v);
void from_json(const nlohmann::json& j, GoalText& v);

void to_json(nlohmann::json& j, const LogsRoot& v);
void from_json(const nlohmann::json& j, LogsRoot& v);

void to_json(nlohmann::json& j, const NodeLabel& v);
void from_json(const nlohmann::json& j, NodeLabel& v);

[[nodiscard]] auto node_shape_to_string(NodeShape s) noexcept -> std::string_view;
[[nodiscard]] auto node_shape_from_string(std::string_view s) noexcept -> std::optional<NodeShape>;

void to_json(nlohmann::json& j, NodeShape v);
void from_json(const nlohmann::json& j, NodeShape& v);

void to_json(nlohmann::json& j, const CssClass& v);
void from_json(const nlohmann::json& j, CssClass& v);

void to_json(nlohmann::json& j, const LlmModel& v);
void from_json(const nlohmann::json& j, LlmModel& v);

void to_json(nlohmann::json& j, const LlmProvider& v);
void from_json(const nlohmann::json& j, LlmProvider& v);

void to_json(nlohmann::json& j, const GraphId& v);
void from_json(const nlohmann::json& j, GraphId& v);

void to_json(nlohmann::json& j, const GraphLabel& v);
void from_json(const nlohmann::json& j, GraphLabel& v);

void to_json(nlohmann::json& j, const StylesheetId& v);
void from_json(const nlohmann::json& j, StylesheetId& v);

void to_json(nlohmann::json& j, const DotfilePath& v);
void from_json(const nlohmann::json& j, DotfilePath& v);

void to_json(nlohmann::json& j, const WorkDir& v);
void from_json(const nlohmann::json& j, WorkDir& v);

void to_json(nlohmann::json& j, const ShellCommand& v);
void from_json(const nlohmann::json& j, ShellCommand& v);

void to_json(nlohmann::json& j, const ConditionExpr& v);
void from_json(const nlohmann::json& j, ConditionExpr& v);

void to_json(nlohmann::json& j, const RuleId& v);
void from_json(const nlohmann::json& j, RuleId& v);

void to_json(nlohmann::json& j, const DiagnosticMessage& v);
void from_json(const nlohmann::json& j, DiagnosticMessage& v);

void to_json(nlohmann::json& j, const SuggestedFix& v);
void from_json(const nlohmann::json& j, SuggestedFix& v);

void to_json(nlohmann::json& j, const HandlerNote& v);
void from_json(const nlohmann::json& j, HandlerNote& v);

void to_json(nlohmann::json& j, const LlmResponse& v);
void from_json(const nlohmann::json& j, LlmResponse& v);

void to_json(nlohmann::json& j, ReasoningEffort v);
void from_json(const nlohmann::json& j, ReasoningEffort& v);

void to_json(nlohmann::json& j, const MaxRetries& v);
void from_json(const nlohmann::json& j, MaxRetries& v);

void to_json(nlohmann::json& j, const Weight& v);
void from_json(const nlohmann::json& j, Weight& v);

void to_json(nlohmann::json& j, const Port& v);
void from_json(const nlohmann::json& j, Port& v);

void to_json(nlohmann::json& j, const TimeoutDuration& v);
void from_json(const nlohmann::json& j, TimeoutDuration& v);

void to_json(nlohmann::json& j, StageStatus v);
void from_json(const nlohmann::json& j, StageStatus& v);

void to_json(nlohmann::json& j, Severity v);
void from_json(const nlohmann::json& j, Severity& v);

void to_json(nlohmann::json& j, QuestionType v);
void from_json(const nlohmann::json& j, QuestionType& v);

void to_json(nlohmann::json& j, AnswerKind v);
void from_json(const nlohmann::json& j, AnswerKind& v);

void to_json(nlohmann::json& j, FidelityMode v);
void from_json(const nlohmann::json& j, FidelityMode& v);

void to_json(nlohmann::json& j, JoinPolicy v);
void from_json(const nlohmann::json& j, JoinPolicy& v);

void to_json(nlohmann::json& j, const MaxParallel& v);
void from_json(const nlohmann::json& j, MaxParallel& v);

}  // namespace attractor

namespace std {

template<>
struct hash<attractor::HandlerTypeName> : type_safe::hashable<attractor::HandlerTypeName> {};

}  // namespace std

#endif  // ATTRACTOR_TYPES_HPP
