#include <attractor/handlers/codergen_handler.hpp>

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <system_error>
#include <type_safe/strong_typedef.hpp>

namespace attractor {

namespace {

constexpr std::string::size_type k_max_context_response_chars = 200;

std::string expand_goal(std::string prompt, const GoalText& goal)
{
    const auto& goal_str = type_safe::get(goal);
    std::string::size_type pos = 0;
    while ((pos = prompt.find("$goal", pos)) != std::string::npos) {
        prompt.replace(pos, 5, goal_str);
        pos += goal_str.size();
    }
    return prompt;
}

void write_status(const std::filesystem::path& stage_dir, const Outcome& outcome)
{
    nlohmann::json j;
    j["outcome"] = outcome.status;
    j["preferred_label"] = type_safe::get(outcome.preferred_label);
    j["context_updates"] = outcome.context_updates;
    j["notes"] = type_safe::get(outcome.notes);
    j["failure_reason"] = type_safe::get(outcome.failure_reason);
    j["suggested_next_ids"] = nlohmann::json::array();
    for (const auto& id : outcome.suggested_next_ids) {
        j["suggested_next_ids"].push_back(type_safe::get(id));
    }
    std::ofstream{stage_dir / "status.json"} << j.dump(2);
}

}  // namespace

CodergenHandler::CodergenHandler(CodergenBackend* backend) : m_backend{backend} {}

auto CodergenHandler::execute(const Node& node, Context& ctx, const Graph& graph, const RunConfig& run_config) const
    -> Outcome
{
    const auto& id_str = type_safe::get(node.id);
    if (id_str.find('/') != std::string::npos || id_str.find("..") != std::string::npos) {
        return Outcome::fail(DiagnosticMessage{"node.id contains path-unsafe characters: " + id_str});
    }

    std::string prompt_str = type_safe::get(node.prompt);
    if (prompt_str.empty()) {
        prompt_str = type_safe::get(node.label);
    }

    prompt_str = expand_goal(std::move(prompt_str), graph.goal);

    if (prompt_str.empty()) {
        return Outcome::fail(DiagnosticMessage{
            "codergen node '" + id_str + "' has no prompt and no label"});
    }

    const auto stage_dir = std::filesystem::path(type_safe::get(run_config.logs_root)) / id_str;
    std::error_code ec;
    std::filesystem::create_directories(stage_dir, ec);
    if (ec) {
        return Outcome::fail(DiagnosticMessage{"Failed to create stage directory: " + ec.message()});
    }

    {
        std::ofstream f{stage_dir / "prompt.md"};
        if (!f) {
            return Outcome::fail(DiagnosticMessage{"Failed to write prompt.md for node: " + id_str});
        }
        f << prompt_str;
    }

    std::string response_text;

    if (m_backend) {
        auto result = m_backend->run(node, PromptText{prompt_str}, ctx);
        if (!result) {
            write_status(stage_dir, result.error());
            return result.error();
        }
        response_text = type_safe::get(*result);
    }
    else {
        response_text = "[Simulated] Response for stage: " + id_str;
    }

    {
        std::ofstream f{stage_dir / "response.md"};
        if (!f) {
            return Outcome::fail(DiagnosticMessage{"Failed to write response.md for node: " + id_str});
        }
        f << response_text;
    }

    const std::string truncated = response_text.size() > k_max_context_response_chars
                                      ? response_text.substr(0, k_max_context_response_chars)
                                      : response_text;

    auto outcome = Outcome{
        .notes = HandlerNote{"Stage completed: " + id_str},
    };
    outcome.context_updates["last_stage"] = id_str;
    outcome.context_updates["last_response"] = truncated;

    write_status(stage_dir, outcome);
    return outcome;
}

}  // namespace attractor
