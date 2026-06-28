#include <attractor/handlers/tool_handler.hpp>

#include <array>
#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_safe/strong_typedef.hpp>
#include <vector>

namespace attractor {

namespace {

// Runs cmd in a shell, capturing stdout. stderr is redirected to stage_dir/stderr.txt.
// stage_dir must already exist. Throws on popen failure, read error, or non-zero exit.
std::string run_popen(std::string_view cmd, const std::filesystem::path& stage_dir)
{
    // Single-quote the stderr path. Attractor log dirs don't contain single quotes.
    const std::string wrapped =
        "( " + std::string(cmd) + " ) 2>'" + (stage_dir / "stderr.txt").string() + "'";
    // NOLINTNEXTLINE(cert-env33-c) -- intentional shell execution for tool nodes
    FILE* raw = popen(wrapped.c_str(), "r");
    if (raw == nullptr) {
        throw std::runtime_error{"ToolHandler: popen failed"};
    }
    std::unique_ptr<FILE, int (*)(FILE*)> pipe{raw, &pclose};
    std::string result;
    std::array<char, 256> buf{};
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe.get()) != nullptr) {
        result += buf.data();
    }
    if (ferror(pipe.get()) != 0) {
        throw std::runtime_error{"ToolHandler: pipe read error"};
    }
    const int rc = pclose(pipe.release());
    if (rc != 0) {
        throw std::runtime_error{"ToolHandler: command failed (exit status " + std::to_string(rc) + ")"};
    }
    return result;
}

std::string expand_goal(std::string s, const GoalText& goal)
{
    const auto& goal_str = type_safe::get(goal);
    std::string::size_type pos = 0;
    while ((pos = s.find("$goal", pos)) != std::string::npos) {
        s.replace(pos, 5, goal_str);
        pos += goal_str.size();
    }
    return s;
}

std::string expand_context(std::string s, const Context& ctx)
{
    const std::string_view prefix = "$context.";
    std::string::size_type pos = 0;
    while ((pos = s.find(prefix, pos)) != std::string::npos) {
        auto end = pos + prefix.size();
        while (end < s.size() &&
               (std::isalnum(static_cast<unsigned char>(s[end])) || s[end] == '.' ||
                s[end] == '_' || s[end] == '-')) {
            ++end;
        }
        const std::string path = s.substr(pos + prefix.size(), end - (pos + prefix.size()));
        if (path.empty()) {
            ++pos;
            continue;
        }

        // Split dot-separated path; first segment is the ContextKey.
        std::vector<std::string> parts;
        std::string::size_type seg_start = 0;
        std::string::size_type dot;
        while ((dot = path.find('.', seg_start)) != std::string::npos) {
            parts.push_back(path.substr(seg_start, dot - seg_start));
            seg_start = dot + 1;
        }
        parts.push_back(path.substr(seg_start));

        nlohmann::json val = ctx.get(ContextKey{parts[0]});
        for (std::size_t i = 1; i < parts.size(); ++i) {
            if (val.is_object() && val.contains(parts[i])) {
                val = val[parts[i]];
            } else {
                val = nlohmann::json{};
                break;
            }
        }

        std::string replacement;
        if (val.is_string()) {
            replacement = val.get<std::string>();
        } else if (!val.is_null() && !val.is_discarded()) {
            replacement = val.dump();
        }

        s.replace(pos, end - pos, replacement);
        pos += replacement.size();
    }
    return s;
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

ToolHandler::ToolHandler(CommandRunner runner) : m_runner{std::move(runner)} {}

auto ToolHandler::execute(const Node& node, Context& ctx, const Graph& graph, const RunConfig& run_config) const
    -> Outcome
{
    const std::string& id_str = type_safe::get(node.id);
    if (id_str.find('/') != std::string::npos || id_str.find("..") != std::string::npos) {
        return Outcome::fail(DiagnosticMessage{"node.id contains path-unsafe characters: " + id_str});
    }

    const auto node_dir = std::filesystem::path(type_safe::get(run_config.logs_root)) /
                          std::format("{:03d}-{}", ctx.current_execution_counter(), id_str);
    const auto& stage_dir = node_dir;
    std::error_code ec;
    std::filesystem::create_directories(stage_dir, ec);
    if (ec) {
        return Outcome::fail(DiagnosticMessage{"failed to create log dir: " + ec.message()});
    }

    // safe: engine dispatches ToolHandler only for ToolNode
    const std::string cmd = expand_context(
        expand_goal(type_safe::get(static_cast<const ToolNode&>(node).tool_command), graph.goal), ctx);
    std::ofstream{stage_dir / "command.txt"} << cmd;

    if (cmd.empty()) {
        const auto outcome = Outcome::fail(DiagnosticMessage{"tool_command is empty"});
        write_status(stage_dir, outcome);
        return outcome;
    }

    try {
        const std::string output = m_runner ? m_runner(cmd) : run_popen(cmd, stage_dir);

        {
            std::ofstream f{stage_dir / "output.txt"};
            f << output;
        }

        const auto last = output.find_last_not_of("\r\n");
        const std::string trimmed = (last == std::string::npos) ? "" : output.substr(0, last + 1);

        Outcome out;
        out.context_updates["tool"]["output"] = trimmed;
        out.notes = HandlerNote{"Tool completed: " + cmd};
        write_status(stage_dir, out);
        return out;
    }
    catch (const std::exception& e) {
        const auto outcome = Outcome::fail(DiagnosticMessage{e.what()});
        write_status(stage_dir, outcome);
        return outcome;
    }
    catch (...) {
        const auto outcome = Outcome::fail(DiagnosticMessage{"unknown exception"});
        write_status(stage_dir, outcome);
        return outcome;
    }
}

}  // namespace attractor
