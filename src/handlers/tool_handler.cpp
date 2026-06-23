#include <attractor/handlers/tool_handler.hpp>

#include <array>
#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
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

}  // namespace

ToolHandler::ToolHandler(CommandRunner runner) : m_runner{std::move(runner)} {}

auto ToolHandler::execute(const Node& node, Context& /*ctx*/, const Graph& /*graph*/,
                          const RunConfig& /*run_config*/) const -> Outcome
{
    const std::string cmd = type_safe::get(node.tool_command);

    if (cmd.empty()) {
        return Outcome::fail(DiagnosticMessage{"ToolHandler: tool_command is empty"});
    }

    try {
        const std::string output = m_runner ? m_runner(cmd) : run_popen(cmd);

        Outcome out;
        out.context_updates["tool.output"] = output;
        out.notes = HandlerNote{"Tool completed: " + cmd};
        return out;
    }
    catch (const std::exception& e) {
        return Outcome::fail(DiagnosticMessage{std::string{"ToolHandler: "} + e.what()});
    }
    catch (...) {
        return Outcome::fail(DiagnosticMessage{"ToolHandler: unknown exception"});
    }
}

}  // namespace attractor
