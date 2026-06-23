#include <attractor/handlers/tool_handler.hpp>

#include <array>
#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_safe/strong_typedef.hpp>

namespace attractor {

namespace {

std::string run_popen(std::string_view cmd)
{
    // NOLINTNEXTLINE(cert-env33-c) -- intentional shell execution for tool nodes
    FILE* raw = popen(std::string(cmd).c_str(), "r");
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

auto ToolHandler::execute(const Node& node, Context& /*ctx*/, const Graph& /*graph*/,
                          const RunConfig& run_config) const -> Outcome
{
    const std::string& id_str = type_safe::get(node.id);
    if (id_str.find('/') != std::string::npos || id_str.find("..") != std::string::npos) {
        return Outcome::fail(DiagnosticMessage{"ToolHandler: node.id contains path-unsafe characters: " + id_str});
    }

    const auto stage_dir = std::filesystem::path(type_safe::get(run_config.logs_root)) / id_str;
    std::error_code ec;
    std::filesystem::create_directories(stage_dir, ec);
    if (ec) {
        return Outcome::fail(DiagnosticMessage{"ToolHandler: failed to create log dir: " + ec.message()});
    }

    const std::string cmd = type_safe::get(node.tool_command);

    if (cmd.empty()) {
        const auto outcome = Outcome::fail(DiagnosticMessage{"ToolHandler: tool_command is empty"});
        write_status(stage_dir, outcome);
        return outcome;
    }

    try {
        const std::string output = m_runner ? m_runner(cmd) : run_popen(cmd);

        {
            std::ofstream f{stage_dir / "output.txt"};
            f << output;
        }

        Outcome out;
        out.context_updates["tool.output"] = output;
        out.notes = HandlerNote{"Tool completed: " + cmd};
        write_status(stage_dir, out);
        return out;
    }
    catch (const std::exception& e) {
        const auto outcome = Outcome::fail(DiagnosticMessage{std::string{"ToolHandler: "} + e.what()});
        write_status(stage_dir, outcome);
        return outcome;
    }
    catch (...) {
        const auto outcome = Outcome::fail(DiagnosticMessage{"ToolHandler: unknown exception"});
        write_status(stage_dir, outcome);
        return outcome;
    }
}

}  // namespace attractor
