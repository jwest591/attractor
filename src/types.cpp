#include <attractor/types.hpp>
#include <nlohmann/json.hpp>
#include <string_view>
#include <utility>

namespace attractor {

// ── String strong typedefs ────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const NodeId& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, NodeId& v) { v = NodeId{j.get<std::string>()}; }

void to_json(nlohmann::json& j, const EdgeLabel& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, EdgeLabel& v) { v = EdgeLabel{j.get<std::string>()}; }

void to_json(nlohmann::json& j, const ArtifactId& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, ArtifactId& v) { v = ArtifactId{j.get<std::string>()}; }

void to_json(nlohmann::json& j, const HandlerTypeName& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, HandlerTypeName& v) { v = HandlerTypeName{j.get<std::string>()}; }

void to_json(nlohmann::json& j, const ContextKey& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, ContextKey& v) { v = ContextKey{j.get<std::string>()}; }

void to_json(nlohmann::json& j, const ThreadId& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, ThreadId& v) { v = ThreadId{j.get<std::string>()}; }

void to_json(nlohmann::json& j, const PromptText& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, PromptText& v) { v = PromptText{j.get<std::string>()}; }

void to_json(nlohmann::json& j, const GoalText& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, GoalText& v) { v = GoalText{j.get<std::string>()}; }

void to_json(nlohmann::json& j, const LogsRoot& v) { j = type_safe::get(v); }

void from_json(const nlohmann::json& j, LogsRoot& v) { v = LogsRoot{j.get<std::string>()}; }

// ── Int constrained types ─────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const MaxRetries& v) { j = v.get_value(); }

void from_json(const nlohmann::json& j, MaxRetries& v) { v = MaxRetries{j.get<int>()}; }

void to_json(nlohmann::json& j, const Weight& v) { j = v.get_value(); }

void from_json(const nlohmann::json& j, Weight& v) { v = Weight{j.get<int>()}; }

void to_json(nlohmann::json& j, const Port& v) { j = v.get_value(); }

void from_json(const nlohmann::json& j, Port& v) { v = Port{j.get<int>()}; }

// ── TimeoutDuration: serialize as int64 milliseconds ─────────────────────────

void to_json(nlohmann::json& j, const TimeoutDuration& v) { j = v.get_value().count(); }

void from_json(const nlohmann::json& j, TimeoutDuration& v)
{
    v = TimeoutDuration{std::chrono::milliseconds{j.get<int64_t>()}};
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
    for (const auto& [enumerator, name] : k_stage_status_map)
        if (enumerator == v) {
            j = name;
            return;
        }
    j = nullptr;
}

void from_json(const nlohmann::json& j, StageStatus& v)
{
    auto s = j.get<std::string>();
    for (const auto& [enumerator, name] : k_stage_status_map)
        if (name == s) {
            v = enumerator;
            return;
        }
    v = k_stage_status_map[0].first;
}

static const std::pair<Severity, std::string_view> k_severity_map[] = {
    {Severity::error,   "error"  },
    {Severity::warning, "warning"},
    {Severity::info,    "info"   },
};

void to_json(nlohmann::json& j, Severity v)
{
    for (const auto& [enumerator, name] : k_severity_map)
        if (enumerator == v) {
            j = name;
            return;
        }
    j = nullptr;
}

void from_json(const nlohmann::json& j, Severity& v)
{
    auto s = j.get<std::string>();
    for (const auto& [enumerator, name] : k_severity_map)
        if (name == s) {
            v = enumerator;
            return;
        }
    v = k_severity_map[0].first;
}

static const std::pair<QuestionType, std::string_view> k_question_type_map[] = {
    {QuestionType::yes_no,          "yes_no"         },
    {QuestionType::multiple_choice, "multiple_choice"},
    {QuestionType::freeform,        "freeform"       },
    {QuestionType::confirmation,    "confirmation"   },
};

void to_json(nlohmann::json& j, QuestionType v)
{
    for (const auto& [enumerator, name] : k_question_type_map)
        if (enumerator == v) {
            j = name;
            return;
        }
    j = nullptr;
}

void from_json(const nlohmann::json& j, QuestionType& v)
{
    auto s = j.get<std::string>();
    for (const auto& [enumerator, name] : k_question_type_map)
        if (name == s) {
            v = enumerator;
            return;
        }
    v = k_question_type_map[0].first;
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
    for (const auto& [enumerator, name] : k_fidelity_mode_map)
        if (enumerator == v) {
            j = name;
            return;
        }
    j = nullptr;
}

void from_json(const nlohmann::json& j, FidelityMode& v)
{
    auto s = j.get<std::string>();
    for (const auto& [enumerator, name] : k_fidelity_mode_map)
        if (name == s) {
            v = enumerator;
            return;
        }
    v = k_fidelity_mode_map[0].first;
}

}  // namespace attractor
