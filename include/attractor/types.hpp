#ifndef ATTRACTOR_TYPES_HPP
#define ATTRACTOR_TYPES_HPP

#include <chrono>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <type_safe/constrained_type.hpp>
#include <type_safe/strong_typedef.hpp>

namespace attractor {

namespace ts = type_safe;

// ── String strong typedefs (AC1) ──────────────────────────────────────────────

struct NodeId
    : ts::strong_typedef<NodeId, std::string>
    , ts::strong_typedef_op::equality_comparison<NodeId>
    , ts::strong_typedef_op::relational_comparison<NodeId> {
    using strong_typedef::strong_typedef;
};

struct EdgeLabel
    : ts::strong_typedef<EdgeLabel, std::string>
    , ts::strong_typedef_op::equality_comparison<EdgeLabel>
    , ts::strong_typedef_op::relational_comparison<EdgeLabel> {
    using strong_typedef::strong_typedef;
};

struct ArtifactId
    : ts::strong_typedef<ArtifactId, std::string>
    , ts::strong_typedef_op::equality_comparison<ArtifactId>
    , ts::strong_typedef_op::relational_comparison<ArtifactId> {
    using strong_typedef::strong_typedef;
};

struct HandlerTypeName
    : ts::strong_typedef<HandlerTypeName, std::string>
    , ts::strong_typedef_op::equality_comparison<HandlerTypeName>
    , ts::strong_typedef_op::relational_comparison<HandlerTypeName> {
    using strong_typedef::strong_typedef;
};

struct ContextKey
    : ts::strong_typedef<ContextKey, std::string>
    , ts::strong_typedef_op::equality_comparison<ContextKey>
    , ts::strong_typedef_op::relational_comparison<ContextKey> {
    using strong_typedef::strong_typedef;
};

struct ThreadId
    : ts::strong_typedef<ThreadId, std::string>
    , ts::strong_typedef_op::equality_comparison<ThreadId>
    , ts::strong_typedef_op::relational_comparison<ThreadId> {
    using strong_typedef::strong_typedef;
};

struct PromptText
    : ts::strong_typedef<PromptText, std::string>
    , ts::strong_typedef_op::equality_comparison<PromptText>
    , ts::strong_typedef_op::relational_comparison<PromptText> {
    using strong_typedef::strong_typedef;
};

struct GoalText
    : ts::strong_typedef<GoalText, std::string>
    , ts::strong_typedef_op::equality_comparison<GoalText>
    , ts::strong_typedef_op::relational_comparison<GoalText> {
    using strong_typedef::strong_typedef;
};

struct LogsRoot
    : ts::strong_typedef<LogsRoot, std::string>
    , ts::strong_typedef_op::equality_comparison<LogsRoot>
    , ts::strong_typedef_op::relational_comparison<LogsRoot> {
    using strong_typedef::strong_typedef;
};

// ── Constrained types (AC2) ───────────────────────────────────────────────────
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

// ── Enum classes ──────────────────────────────────────────────────────────────

enum class StageStatus { success, partial_success, fail, retry, skipped };

enum class Severity { error, warning, info };

enum class QuestionType { yes_no, multiple_choice, freeform, confirmation };

enum class FidelityMode { full, truncate, compact, summary_low, summary_medium, summary_high };

// ── JSON serialization declarations (defined in src/types.cpp) ───────────────

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

void to_json(nlohmann::json& j, FidelityMode v);
void from_json(const nlohmann::json& j, FidelityMode& v);

}  // namespace attractor

#endif  // ATTRACTOR_TYPES_HPP
