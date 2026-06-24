#include "claude_tmux_backend.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <snitch/snitch.hpp>
#include <type_safe/strong_typedef.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>

#include <cstdlib>
#include <unistd.h>

using namespace attractor;

namespace {

bool tmux_available()
{
    return system("command -v tmux >/dev/null 2>&1") == 0;  // NOLINT(cert-env33-c)
}

bool jq_available()
{
    return system("command -v jq >/dev/null 2>&1") == 0;  // NOLINT(cert-env33-c)
}

[[nodiscard]] bool prerequisites_ok()
{
    return tmux_available() && jq_available();
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
struct TmpDir {
    explicit TmpDir(std::filesystem::path p) : path{std::move(p)}
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }
    TmpDir(const TmpDir&) = delete;
    TmpDir& operator=(const TmpDir&) = delete;
    ~TmpDir() noexcept
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    std::filesystem::path path;
};

void write_script(const std::filesystem::path& p, const std::string& content)
{
    {
        std::ofstream f{p};
        f << content;
    }
    std::filesystem::permissions(p, std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read |
                                        std::filesystem::perms::owner_write);
}

// Inject mock_dir into PATH for all windows created on the given tmux server.
// Uses set-option default-command so the shell starts with mock_dir prepended to PATH
// before bash startup files can push it back; this is necessary because bash login shells
// source .profile/.bashrc which prepend other entries, burying a process-env PATH injection.
void inject_mock_path(const std::string& tmux_cmd, const std::string& mock_dir)
{
    const std::string default_cmd = "export PATH=" + mock_dir + ":$PATH; exec bash -i";
    const std::string full_cmd = tmux_cmd + " set-option -g default-command '" + default_cmd + "'";
    static_cast<void>(system(full_cmd.c_str()));  // NOLINT(cert-env33-c)
}

[[nodiscard]] std::string generate_success_mock_claude(const std::string& scripts_dir)
{
    return std::string{"#!/usr/bin/env bash\nset -eu\nSCRIPTS_DIR=\""} + scripts_dir +
           "\"\n"
           "JSONL=\"$ATTRACTOR_NODE_LOG_DIR/session.jsonl\"\n"
           "touch \"$JSONL\"\n"
           "printf '{\"transcript_path\":\"%s\",\"session_title\":\"mock\",\"session_id\":\"test\"}\\n'"
           " \"$JSONL\" | ATTRACTOR_NODE_LOG_DIR=\"$ATTRACTOR_NODE_LOG_DIR\""
           " \"$SCRIPTS_DIR/att-session-start.sh\" >/dev/null 2>&1\n"
           "printf '{\"status\":\"ok\",\"message\":\"integration ok\"}\\n'"
           " > \"$ATTRACTOR_NODE_LOG_DIR/done.json\"\n";
}

[[nodiscard]] std::string generate_error_mock_claude(const std::string& scripts_dir)
{
    return std::string{"#!/usr/bin/env bash\nset -eu\nSCRIPTS_DIR=\""} + scripts_dir +
           "\"\n"
           "JSONL=\"$ATTRACTOR_NODE_LOG_DIR/session.jsonl\"\n"
           "touch \"$JSONL\"\n"
           "printf '{\"transcript_path\":\"%s\",\"session_title\":\"mock\",\"session_id\":\"test\"}\\n'"
           " \"$JSONL\" | ATTRACTOR_NODE_LOG_DIR=\"$ATTRACTOR_NODE_LOG_DIR\""
           " \"$SCRIPTS_DIR/att-session-start.sh\" >/dev/null 2>&1\n"
           "printf '{\"status\":\"error\",\"error_type\":\"api_error\",\"message\":\"mock failure\"}\\n'"
           " > \"$ATTRACTOR_NODE_LOG_DIR/done.json\"\n";
}

[[nodiscard]] std::string generate_timeout_mock_claude(const std::string& /*scripts_dir*/)
{
    // Does not write transcript.txt or done.json; backend times out at SessionStart deadline.
    return "#!/usr/bin/env bash\nset -eu\n";
}

}  // namespace

SNITCH_TEST_CASE("[tmux_integration] success path: done.json ok returns LlmResponse -- 7.11-I-001")
{
    if (!prerequisites_ok()) {
        std::println(stderr, "SKIP 7.11-I-001: tmux or jq not available in PATH");
        return;
    }
    const auto pid = std::to_string(::getpid());
    TmpDir mock_dir{std::filesystem::temp_directory_path() / ("att_tmux_it_001_" + pid)};
    TmpDir logs_root{std::filesystem::temp_directory_path() / ("att_tmux_logs_001_" + pid)};
    write_script(mock_dir.path / "claude", generate_success_mock_claude(ATTRACTOR_CLI_SCRIPTS_DIR));
    const std::string tmux_cmd = "tmux -L tmux_it_001_" + pid;
    ClaudeCodeTmuxBackend backend{tmux_cmd, logs_root.path};
    inject_mock_path(tmux_cmd, mock_dir.path.string());
    Node node{};
    node.id = NodeId{"test"};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);
    auto result = backend.run(node, PromptText{"hello"}, ctx);
    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "integration ok");
}

SNITCH_TEST_CASE("[tmux_integration] done.json error status returns Outcome::fail -- 7.11-I-002")
{
    if (!prerequisites_ok()) {
        std::println(stderr, "SKIP 7.11-I-002: tmux or jq not available in PATH");
        return;
    }
    const auto pid = std::to_string(::getpid());
    TmpDir mock_dir{std::filesystem::temp_directory_path() / ("att_tmux_it_002_" + pid)};
    TmpDir logs_root{std::filesystem::temp_directory_path() / ("att_tmux_logs_002_" + pid)};
    write_script(mock_dir.path / "claude", generate_error_mock_claude(ATTRACTOR_CLI_SCRIPTS_DIR));
    const std::string tmux_cmd = "tmux -L tmux_it_002_" + pid;
    ClaudeCodeTmuxBackend backend{tmux_cmd, logs_root.path};
    inject_mock_path(tmux_cmd, mock_dir.path.string());
    Node node{};
    node.id = NodeId{"test"};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);
    auto result = backend.run(node, PromptText{"hello"}, ctx);
    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(result.error().status == StageStatus::fail);
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("api_error") != std::string::npos);
}

SNITCH_TEST_CASE("[tmux_integration] SessionStart timeout returns Outcome::fail -- 7.11-I-003")
{
    if (!prerequisites_ok()) {
        std::println(stderr, "SKIP 7.11-I-003: tmux or jq not available in PATH");
        return;
    }
    const auto pid = std::to_string(::getpid());
    TmpDir mock_dir{std::filesystem::temp_directory_path() / ("att_tmux_it_003_" + pid)};
    TmpDir logs_root{std::filesystem::temp_directory_path() / ("att_tmux_logs_003_" + pid)};
    write_script(mock_dir.path / "claude", generate_timeout_mock_claude(ATTRACTOR_CLI_SCRIPTS_DIR));
    const std::string tmux_cmd = "tmux -L tmux_it_003_" + pid;
    ClaudeCodeTmuxBackend backend{tmux_cmd, logs_root.path};
    inject_mock_path(tmux_cmd, mock_dir.path.string());
    Node node{};
    node.id = NodeId{"test"};
    node.timeout = TimeoutDuration{std::chrono::milliseconds{2000}};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);
    auto result = backend.run(node, PromptText{"hello"}, ctx);
    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(result.error().status == StageStatus::fail);
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("timeout") != std::string::npos);
}
