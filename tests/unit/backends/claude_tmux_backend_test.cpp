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
#include <string>

using namespace attractor;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
struct TmpDir {
    explicit TmpDir(std::filesystem::path p) : path{std::move(p)} {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }
    TmpDir(const TmpDir&) = delete;
    TmpDir& operator=(const TmpDir&) = delete;
    ~TmpDir() noexcept { std::error_code ec; std::filesystem::remove_all(path, ec); }
    std::filesystem::path path;
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
struct TmpFile {
    explicit TmpFile(std::filesystem::path p) : path{std::move(p)} {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    TmpFile(const TmpFile&) = delete;
    TmpFile& operator=(const TmpFile&) = delete;
    ~TmpFile() noexcept { std::error_code ec; std::filesystem::remove(path, ec); }
    std::filesystem::path path;
};

void write_script(const std::filesystem::path& p, const std::string& content)
{
    { std::ofstream f{p}; f << content; }
    std::filesystem::permissions(p,
        std::filesystem::perms::owner_exec |
        std::filesystem::perms::owner_read |
        std::filesystem::perms::owner_write);
}

// Mock tmux handler that parses -e ATTRACTOR_NODE_LOG_DIR=<path> from new-window args.
// Used by success/error tests; timeout tests omit transcript.txt or stop_reason.
constexpr const char* k_mock_preamble = R"(#!/usr/bin/env bash
set -u
case "$1" in
  new-session) exit 0 ;;
  kill-session) exit 0 ;;
  kill-window)  exit 0 ;;
  send-keys)    exit 0 ;;
  new-window)
    shift
    NDL=""
    while [ "$#" -gt 0 ]; do
      if [ "$1" = "-e" ] && [ "$#" -gt 1 ]; then
        shift
        case "$1" in
          ATTRACTOR_NODE_LOG_DIR=*) NDL="${1#ATTRACTOR_NODE_LOG_DIR=}" ;;
        esac
      fi
      shift
    done
)";
constexpr const char* k_mock_suffix = R"(    exit 0
    ;;
esac
exit 1
)";

}  // namespace

SNITCH_TEST_CASE("[claude_tmux] single run returns LlmResponse on stop_reason -- 7.19-U-001")
{
    auto script = std::filesystem::temp_directory_path() / "att_tmux_7_19_001.sh";
    TmpFile g_script{script};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_7_19_001"};

    // new-window: create node_log_dir, write transcript.txt + JSONL with stop_reason
    write_script(script,
        std::string{k_mock_preamble} +
        R"(    if [ -n "$NDL" ]; then
      mkdir -p "$NDL"
      printf '%s/transcript.jsonl\n' "$NDL" > "$NDL/transcript.txt"
      printf '{"type":"assistant","message":{"role":"assistant","content":[{"type":"text","text":"mock response"}],"stop_reason":"end_turn"}}\n' > "$NDL/transcript.jsonl"
    fi
)" + k_mock_suffix);

    ClaudeCodeTmuxBackend backend{script.string(), logs_root.path};
    Node node{};
    node.id = NodeId{"n1"};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);  // simulate HandoffAwareBackend increment

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "mock response");
}

SNITCH_TEST_CASE("[claude_tmux] SessionStart timeout returns fail -- 7.19-U-002")
{
    auto script = std::filesystem::temp_directory_path() / "att_tmux_7_19_002.sh";
    TmpFile g_script{script};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_7_19_002"};

    // new-window: create dir but do NOT write transcript.txt -> polling times out
    write_script(script,
        std::string{k_mock_preamble} +
        R"(    if [ -n "$NDL" ]; then
      mkdir -p "$NDL"
    fi
)" + k_mock_suffix);

    ClaudeCodeTmuxBackend backend{script.string(), logs_root.path};
    Node node{};
    node.id = NodeId{"n2"};
    node.timeout = TimeoutDuration{std::chrono::milliseconds{300}};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("timeout") != std::string::npos);
    SNITCH_CHECK(result.error().status == StageStatus::fail);
}

SNITCH_TEST_CASE("[claude_tmux] JSONL error type returns fail -- 7.19-U-003")
{
    auto script = std::filesystem::temp_directory_path() / "att_tmux_7_19_003.sh";
    TmpFile g_script{script};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_7_19_003"};

    // new-window: write transcript.txt + JSONL with "type":"error"
    write_script(script,
        std::string{k_mock_preamble} +
        R"(    if [ -n "$NDL" ]; then
      mkdir -p "$NDL"
      printf '%s/transcript.jsonl\n' "$NDL" > "$NDL/transcript.txt"
      printf '{"type":"error","error":{"type":"api_error","message":"internal server error"}}\n' > "$NDL/transcript.jsonl"
    fi
)" + k_mock_suffix);

    ClaudeCodeTmuxBackend backend{script.string(), logs_root.path};
    Node node{};
    node.id = NodeId{"n3"};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(result.error().status == StageStatus::fail);
}

SNITCH_TEST_CASE("[claude_tmux] node.timeout deadline returns fail -- 7.19-U-004")
{
    auto script = std::filesystem::temp_directory_path() / "att_tmux_7_19_004.sh";
    TmpFile g_script{script};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_7_19_004"};

    // new-window: write transcript.txt + JSONL but never include stop_reason
    write_script(script,
        std::string{k_mock_preamble} +
        R"(    if [ -n "$NDL" ]; then
      mkdir -p "$NDL"
      printf '%s/transcript.jsonl\n' "$NDL" > "$NDL/transcript.txt"
      printf '{"type":"system","subtype":"init","session_id":"abc"}\n' > "$NDL/transcript.jsonl"
    fi
)" + k_mock_suffix);

    ClaudeCodeTmuxBackend backend{script.string(), logs_root.path};
    Node node{};
    node.id = NodeId{"n4"};
    node.timeout = TimeoutDuration{std::chrono::milliseconds{300}};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(result.error().status == StageStatus::fail);
}
