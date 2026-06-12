#include <attractor/types.hpp>
#include <nlohmann/json.hpp>
#include <string_view>
#include <utility>

namespace attractor {

// ── String strong typedefs ────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const NodeId& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, NodeId& v)
{
    if (!j.is_string()) {
        throw nlohmann::json::type_error::create(302, "NodeId requires a string, got: " + std::string{j.type_name()},
                                                 &j);
    }
    v = NodeId{j.get<std::string>()};
}

void to_json(nlohmann::json& j, const EdgeLabel& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, EdgeLabel& v)
{
    if (!j.is_string()) {
        throw nlohmann::json::type_error::create(302, "EdgeLabel requires a string, got: " + std::string{j.type_name()},
                                                 &j);
    }
    v = EdgeLabel{j.get<std::string>()};
}

void to_json(nlohmann::json& j, const ArtifactId& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, ArtifactId& v)
{
    if (!j.is_string()) {
        throw nlohmann::json::type_error::create(
            302, "ArtifactId requires a string, got: " + std::string{j.type_name()}, &j);
    }
    v = ArtifactId{j.get<std::string>()};
}

void to_json(nlohmann::json& j, const HandlerTypeName& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, HandlerTypeName& v)
{
    if (!j.is_string()) {
        throw nlohmann::json::type_error::create(
            302, "HandlerTypeName requires a string, got: " + std::string{j.type_name()}, &j);
    }
    v = HandlerTypeName{j.get<std::string>()};
}

void to_json(nlohmann::json& j, const ContextKey& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, ContextKey& v)
{
    if (!j.is_string()) {
        throw nlohmann::json::type_error::create(
            302, "ContextKey requires a string, got: " + std::string{j.type_name()}, &j);
    }
    v = ContextKey{j.get<std::string>()};
}

void to_json(nlohmann::json& j, const ThreadId& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, ThreadId& v)
{
    if (!j.is_string()) {
        throw nlohmann::json::type_error::create(302, "ThreadId requires a string, got: " + std::string{j.type_name()},
                                                 &j);
    }
    v = ThreadId{j.get<std::string>()};
}

void to_json(nlohmann::json& j, const PromptText& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, PromptText& v)
{
    if (!j.is_string()) {
        throw nlohmann::json::type_error::create(
            302, "PromptText requires a string, got: " + std::string{j.type_name()}, &j);
    }
    v = PromptText{j.get<std::string>()};
}

void to_json(nlohmann::json& j, const GoalText& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, GoalText& v)
{
    if (!j.is_string()) {
        throw nlohmann::json::type_error::create(302, "GoalText requires a string, got: " + std::string{j.type_name()},
                                                 &j);
    }
    v = GoalText{j.get<std::string>()};
}

void to_json(nlohmann::json& j, const LogsRoot& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, LogsRoot& v)
{
    if (!j.is_string()) {
        throw nlohmann::json::type_error::create(302, "LogsRoot requires a string, got: " + std::string{j.type_name()},
                                                 &j);
    }
    v = LogsRoot{j.get<std::string>()};
}

#define ATTRACTOR_STRING_TYPEDEF_JSON(Type)                                                                            \
    void to_json(nlohmann::json& j, const Type& v) { j = type_safe::get(v); }                                          \
    void from_json(const nlohmann::json& j, Type& v)                                                                   \
    {                                                                                                                  \
        if (!j.is_string())                                                                                            \
            throw nlohmann::json::type_error::create(                                                                  \
                302, #Type " requires a string, got: " + std::string{j.type_name()}, &j);                              \
        v = Type{j.get<std::string>()};                                                                                \
    }

ATTRACTOR_STRING_TYPEDEF_JSON(NodeLabel)
ATTRACTOR_STRING_TYPEDEF_JSON(CssClass)
ATTRACTOR_STRING_TYPEDEF_JSON(LlmModel)
ATTRACTOR_STRING_TYPEDEF_JSON(LlmProvider)
ATTRACTOR_STRING_TYPEDEF_JSON(GraphId)
ATTRACTOR_STRING_TYPEDEF_JSON(GraphLabel)
ATTRACTOR_STRING_TYPEDEF_JSON(StylesheetId)
ATTRACTOR_STRING_TYPEDEF_JSON(DotfilePath)
ATTRACTOR_STRING_TYPEDEF_JSON(WorkDir)
ATTRACTOR_STRING_TYPEDEF_JSON(ShellCommand)
ATTRACTOR_STRING_TYPEDEF_JSON(ConditionExpr)
ATTRACTOR_STRING_TYPEDEF_JSON(RuleId)
ATTRACTOR_STRING_TYPEDEF_JSON(DiagnosticMessage)
ATTRACTOR_STRING_TYPEDEF_JSON(SuggestedFix)
ATTRACTOR_STRING_TYPEDEF_JSON(HandlerNote)
ATTRACTOR_STRING_TYPEDEF_JSON(LlmResponse)

#undef ATTRACTOR_STRING_TYPEDEF_JSON

// ── Int constrained types ─────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const MaxRetries& v) { j = v.get_value(); }

void from_json(const nlohmann::json& j, MaxRetries& v)
{
    auto val = j.get<int>();
    if (!max_retries_constraint{}(val)) {
        throw nlohmann::json::other_error::create(501, "MaxRetries value must be >= 0, got: " + std::to_string(val),
                                                  &j);
    }
    v = MaxRetries{val};
}

void to_json(nlohmann::json& j, const Weight& v) { j = v.get_value(); }

void from_json(const nlohmann::json& j, Weight& v)
{
    auto val = j.get<int>();
    if (!weight_constraint{}(val)) {
        throw nlohmann::json::other_error::create(501, "Weight value must be >= 0, got: " + std::to_string(val), &j);
    }
    v = Weight{val};
}

void to_json(nlohmann::json& j, const Port& v) { j = v.get_value(); }

void from_json(const nlohmann::json& j, Port& v)
{
    auto val = j.get<int>();
    if (!port_constraint{}(val)) {
        throw nlohmann::json::other_error::create(501, "Port value must be 1-65535, got: " + std::to_string(val), &j);
    }
    v = Port{val};
}

// ── TimeoutDuration: serialize as int64 milliseconds ─────────────────────────

void to_json(nlohmann::json& j, const TimeoutDuration& v) { j = v.get_value().count(); }

void from_json(const nlohmann::json& j, TimeoutDuration& v)
{
    auto ms = std::chrono::milliseconds{j.get<int64_t>()};
    if (!positive_duration_constraint{}(ms)) {
        throw nlohmann::json::other_error::create(
            501, "TimeoutDuration must be > 0 ms, got: " + std::to_string(ms.count()), &j);
    }
    v = TimeoutDuration{ms};
}

// ── Enum classes ──────────────────────────────────────────────────────────────

static const std::pair<StageStatus, std::string_view> k_stage_status_map[] = {
    {StageStatus::success,         "success"        },
    {StageStatus::partial_success, "partial_success"},
    {StageStatus::fail,            "fail"           },
    {StageStatus::retry,           "retry"          },
    {StageStatus::skipped,         "skipped"        },
};

void to_json(nlohmann::json& j, StageStatus v)
{
    for (const auto& [enumerator, name] : k_stage_status_map) {
        if (enumerator == v) {
            j = name;
            return;
        }
    }
    j = nullptr;
}

void from_json(const nlohmann::json& j, StageStatus& v)
{
    auto s = j.get<std::string>();
    for (const auto& [enumerator, name] : k_stage_status_map) {
        if (name == s) {
            v = enumerator;
            return;
        }
    }
    throw nlohmann::json::other_error::create(501, "unknown StageStatus: " + s, &j);
}

static const std::pair<Severity, std::string_view> k_severity_map[] = {
    {Severity::error,   "error"  },
    {Severity::warning, "warning"},
    {Severity::info,    "info"   },
};

void to_json(nlohmann::json& j, Severity v)
{
    for (const auto& [enumerator, name] : k_severity_map) {
        if (enumerator == v) {
            j = name;
            return;
        }
    }
    j = nullptr;
}

void from_json(const nlohmann::json& j, Severity& v)
{
    auto s = j.get<std::string>();
    for (const auto& [enumerator, name] : k_severity_map) {
        if (name == s) {
            v = enumerator;
            return;
        }
    }
    throw nlohmann::json::other_error::create(501, "unknown Severity: " + s, &j);
}

static const std::pair<QuestionType, std::string_view> k_question_type_map[] = {
    {QuestionType::yes_no,          "yes_no"         },
    {QuestionType::multiple_choice, "multiple_choice"},
    {QuestionType::freeform,        "freeform"       },
    {QuestionType::confirmation,    "confirmation"   },
};

void to_json(nlohmann::json& j, QuestionType v)
{
    for (const auto& [enumerator, name] : k_question_type_map) {
        if (enumerator == v) {
            j = name;
            return;
        }
    }
    j = nullptr;
}

void from_json(const nlohmann::json& j, QuestionType& v)
{
    auto s = j.get<std::string>();
    for (const auto& [enumerator, name] : k_question_type_map) {
        if (name == s) {
            v = enumerator;
            return;
        }
    }
    throw nlohmann::json::other_error::create(501, "unknown QuestionType: " + s, &j);
}

static const std::pair<NodeShape, std::string_view> k_node_shape_map[] = {
    {NodeShape::mdiamond,      "Mdiamond"     },
    {NodeShape::msquare,       "Msquare"      },
    {NodeShape::box,           "box"          },
    {NodeShape::hexagon,       "hexagon"      },
    {NodeShape::diamond,       "diamond"      },
    {NodeShape::component,     "component"    },
    {NodeShape::triple_octagon,"tripleoctagon"},
    {NodeShape::parallelogram, "parallelogram"},
    {NodeShape::house,         "house"        },
};

auto node_shape_to_string(NodeShape s) noexcept -> std::string_view
{
    for (const auto& [enumerator, name] : k_node_shape_map) {
        if (enumerator == s) {
            return name;
        }
    }
    std::unreachable();
}

auto node_shape_from_string(std::string_view s) noexcept -> std::optional<NodeShape>
{
    for (const auto& [enumerator, name] : k_node_shape_map) {
        if (name == s) {
            return enumerator;
        }
    }
    return std::nullopt;
}

void to_json(nlohmann::json& j, NodeShape v)
{
    j = node_shape_to_string(v);
}

void from_json(const nlohmann::json& j, NodeShape& v)
{
    auto s = j.get<std::string>();
    if (auto parsed = node_shape_from_string(s)) {
        v = *parsed;
        return;
    }
    throw nlohmann::json::other_error::create(501, "unknown NodeShape: " + s, &j);
}

static const std::pair<AnswerKind, std::string_view> k_answer_kind_map[] = {
    {AnswerKind::yes,     "yes"    },
    {AnswerKind::no,      "no"     },
    {AnswerKind::text,    "text"   },
    {AnswerKind::skipped, "skipped"},
    {AnswerKind::timeout, "timeout"},
};

void to_json(nlohmann::json& j, AnswerKind v)
{
    for (const auto& [enumerator, name] : k_answer_kind_map) {
        if (enumerator == v) {
            j = name;
            return;
        }
    }
    j = nullptr;
}

void from_json(const nlohmann::json& j, AnswerKind& v)
{
    auto s = j.get<std::string>();
    for (const auto& [enumerator, name] : k_answer_kind_map) {
        if (name == s) {
            v = enumerator;
            return;
        }
    }
    throw nlohmann::json::other_error::create(501, "unknown AnswerKind: " + s, &j);
}

static const std::pair<FidelityMode, std::string_view> k_fidelity_mode_map[] = {
    {FidelityMode::full,           "full"          },
    {FidelityMode::truncate,       "truncate"      },
    {FidelityMode::compact,        "compact"       },
    {FidelityMode::summary_low,    "summary:low"   },
    {FidelityMode::summary_medium, "summary:medium"},
    {FidelityMode::summary_high,   "summary:high"  },
};

void to_json(nlohmann::json& j, FidelityMode v)
{
    for (const auto& [enumerator, name] : k_fidelity_mode_map) {
        if (enumerator == v) {
            j = name;
            return;
        }
    }
    j = nullptr;
}

void from_json(const nlohmann::json& j, FidelityMode& v)
{
    auto s = j.get<std::string>();
    for (const auto& [enumerator, name] : k_fidelity_mode_map) {
        if (name == s) {
            v = enumerator;
            return;
        }
    }
    throw nlohmann::json::other_error::create(501, "unknown FidelityMode: " + s, &j);
}

static const std::pair<ReasoningEffort, std::string_view> k_reasoning_effort_map[] = {
    {ReasoningEffort::low,    "low"   },
    {ReasoningEffort::medium, "medium"},
    {ReasoningEffort::high,   "high"  },
};

void to_json(nlohmann::json& j, ReasoningEffort v)
{
    for (const auto& [enumerator, name] : k_reasoning_effort_map) {
        if (enumerator == v) {
            j = name;
            return;
        }
    }
    j = nullptr;
}

void from_json(const nlohmann::json& j, ReasoningEffort& v)
{
    auto s = j.get<std::string>();
    for (const auto& [enumerator, name] : k_reasoning_effort_map) {
        if (name == s) {
            v = enumerator;
            return;
        }
    }
    throw nlohmann::json::other_error::create(501, "unknown ReasoningEffort: " + s, &j);
}

}  // namespace attractor
