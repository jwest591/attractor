#include "backend_utils.hpp"
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
// Used by success/error tests; timeout tests omit transcript.txt or done.json.
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

SNITCH_TEST_CASE("[claude_tmux] single run returns LlmResponse on done.json ok -- 7.19-U-001")
{
    auto script = std::filesystem::temp_directory_path() / "att_tmux_7_19_001.sh";
    TmpFile g_script{script};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_7_19_001"};

    // new-window: create node_log_dir, write transcript.txt + done.json with status ok
    write_script(script,
        std::string{k_mock_preamble} +
        R"(    if [ -n "$NDL" ]; then
      mkdir -p "$NDL"
      printf 'started\n' > "$NDL/transcript.txt"
      printf '{"status":"ok","message":"mock response"}\n' > "$NDL/done.json"
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

SNITCH_TEST_CASE("[claude_tmux] done.json error status returns fail -- 7.19-U-003")
{
    auto script = std::filesystem::temp_directory_path() / "att_tmux_7_19_003.sh";
    TmpFile g_script{script};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_7_19_003"};

    // new-window: write transcript.txt + done.json with status error
    write_script(script,
        std::string{k_mock_preamble} +
        R"(    if [ -n "$NDL" ]; then
      mkdir -p "$NDL"
      printf 'started\n' > "$NDL/transcript.txt"
      printf '{"status":"error","error_type":"server_error","message":"internal server error"}\n' > "$NDL/done.json"
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

    // new-window: write transcript.txt but never write done.json -> deadline expires
    write_script(script,
        std::string{k_mock_preamble} +
        R"(    if [ -n "$NDL" ]; then
      mkdir -p "$NDL"
      printf 'started\n' > "$NDL/transcript.txt"
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

SNITCH_TEST_CASE("[claude_tmux] ceiling disabled: high-token JSONL does not trigger handoff -- 7.12-U-001")
{
    auto script = std::filesystem::temp_directory_path() / "att_tmux_7_12_001.sh";
    TmpFile g_script{script};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_7_12_001"};

    // new-window: write JSONL with 200k tokens, transcript.txt pointing to it, and done.json ok.
    // With ceiling_tokens=0 (disabled) the backend must ignore the usage event and return ok.
    write_script(script,
        std::string{k_mock_preamble} +
        R"(    if [ -n "$NDL" ]; then
      mkdir -p "$NDL"
      printf '{"type":"usage","input_tokens":200000}\n' > "$NDL/session.jsonl"
      printf '%s/session.jsonl\n' "$NDL" > "$NDL/transcript.txt"
      printf '{"status":"ok","message":"ceiling disabled ok"}\n' > "$NDL/done.json"
    fi
)" + k_mock_suffix);

    ClaudeCodeTmuxBackend backend{script.string(), logs_root.path, 0, 5};
    Node node{};
    node.id = NodeId{"n5"};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "ceiling disabled ok");
}

SNITCH_TEST_CASE("[claude_tmux] ceiling enabled: tokens below threshold do not trigger handoff -- 7.12-U-002")
{
    auto script = std::filesystem::temp_directory_path() / "att_tmux_7_12_002.sh";
    TmpFile g_script{script};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_7_12_002"};

    // new-window: write JSONL with 100k tokens (below 160k ceiling), transcript.txt, and done.json ok.
    write_script(script,
        std::string{k_mock_preamble} +
        R"(    if [ -n "$NDL" ]; then
      mkdir -p "$NDL"
      printf '{"type":"usage","input_tokens":100000}\n' > "$NDL/session.jsonl"
      printf '%s/session.jsonl\n' "$NDL" > "$NDL/transcript.txt"
      printf '{"status":"ok","message":"below ceiling ok"}\n' > "$NDL/done.json"
    fi
)" + k_mock_suffix);

    ClaudeCodeTmuxBackend backend{script.string(), logs_root.path, 160'000, 5};
    Node node{};
    node.id = NodeId{"n6"};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "below ceiling ok");
}

SNITCH_TEST_CASE("[claude_tmux] ceiling exceeded with max_handoffs=0 returns fail immediately -- 7.12-U-003")
{
    auto script = std::filesystem::temp_directory_path() / "att_tmux_7_12_003.sh";
    TmpFile g_script{script};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_7_12_003"};

    // new-window: write JSONL with 170k tokens (above 160k ceiling), transcript.txt, but NO done.json.
    // With max_ceiling_handoffs=0, the backend must detect the ceiling and return FAIL immediately
    // without waiting for done.json or attempting a /clear handoff.
    write_script(script,
        std::string{k_mock_preamble} +
        R"(    if [ -n "$NDL" ]; then
      mkdir -p "$NDL"
      printf '{"type":"usage","input_tokens":170000}\n' > "$NDL/session.jsonl"
      printf '%s/session.jsonl\n' "$NDL" > "$NDL/transcript.txt"
    fi
)" + k_mock_suffix);

    ClaudeCodeTmuxBackend backend{script.string(), logs_root.path, 160'000, 0};
    Node node{};
    node.id = NodeId{"n7"};
    node.timeout = TimeoutDuration{std::chrono::milliseconds{500}};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(result.error().status == StageStatus::fail);
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("exhausted") != std::string::npos);
}

// ---------------------------------------------------------------------------
// parse_rate_limit_reset unit tests
// ---------------------------------------------------------------------------

SNITCH_TEST_CASE("[backend_utils] parse_rate_limit_reset returns time and tz for valid message -- rate-limit-P-001")
{
    const auto result = parse_rate_limit_reset(
        "You've hit your session limit resets 8:30pm (Europe/London)");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(result->first == "8:30pm");
    SNITCH_CHECK(result->second == "Europe/London");
}

SNITCH_TEST_CASE("[backend_utils] parse_rate_limit_reset handles am suffix -- rate-limit-P-002")
{
    const auto result = parse_rate_limit_reset("session limit resets 12:00am (America/New_York)");
    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(result->first == "12:00am");
    SNITCH_CHECK(result->second == "America/New_York");
}

SNITCH_TEST_CASE("[backend_utils] parse_rate_limit_reset returns nullopt for message without reset time -- rate-limit-P-003")
{
    SNITCH_CHECK_FALSE(parse_rate_limit_reset("You've hit your session limit").has_value());
    SNITCH_CHECK_FALSE(parse_rate_limit_reset("").has_value());
    SNITCH_CHECK_FALSE(parse_rate_limit_reset("resets 8:30 (Europe/London)").has_value());
}

// ---------------------------------------------------------------------------
// Rate limit recovery / exhaustion behavior tests
//
// k_mock_preamble intercepts send-keys) exit 0 before any custom handler, so
// these tests use a custom preamble that omits that case and handles send-keys
// themselves with an Enter-count-based state machine.
// ---------------------------------------------------------------------------

// Like k_mock_preamble but without the send-keys) exit 0 ;; line so the
// caller can supply its own send-keys) handler below new-window).
constexpr const char* k_mock_preamble_no_sk = R"(#!/usr/bin/env bash
set -u
case "$1" in
  new-session) exit 0 ;;
  kill-session) exit 0 ;;
  kill-window)  exit 0 ;;
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

// Builds a full rate-limit mock script. new-window writes transcript.txt +
// the initial done.json, and saves the NDL path to ndl_file.
// send-keys Enter counts invocations (counter_file). When the count reaches
// enter_threshold, it writes recovery_transcript_line to transcript.txt and
// recovery_done_json to done.json.
std::string make_rate_limit_script(const std::string& ndl_file,
                                   const std::string& counter_file,
                                   const std::string& initial_done_json,
                                   int enter_threshold,
                                   const std::string& recovery_transcript_line,
                                   const std::string& recovery_done_json)
{
    return std::string{k_mock_preamble_no_sk} +
        "    if [ -n \"$NDL\" ]; then\n"
        "      mkdir -p \"$NDL\"\n"
        "      printf '%s\\n' \"$NDL\" > '" + ndl_file + "'\n"
        "      printf 'started\\n' > \"$NDL/transcript.txt\"\n"
        "      printf '%s\\n' '" + initial_done_json + "' > \"$NDL/done.json\"\n"
        "      printf '0' > '" + counter_file + "'\n"
        "    fi\n"
        "    exit 0\n"
        "    ;;\n"
        "  send-keys)\n"
        "    if [ \"${@: -1}\" = \"Enter\" ]; then\n"
        "      count=$(cat '" + counter_file + "' 2>/dev/null || printf '0')\n"
        "      count=$((count + 1))\n"
        "      printf '%s' \"$count\" > '" + counter_file + "'\n"
        "      if [ \"$count\" -ge " + std::to_string(enter_threshold) + " ]; then\n"
        "        ndl=$(cat '" + ndl_file + "')\n"
        "        printf '%s\\n' '" + recovery_transcript_line + "' > \"$ndl/transcript.txt\"\n"
        "        printf '%s\\n' '" + recovery_done_json + "' > \"$ndl/done.json\"\n"
        "      fi\n"
        "    fi\n"
        "    exit 0\n"
        "    ;;\n"
        "esac\n"
        "exit 1\n";
}

SNITCH_TEST_CASE("[claude_tmux] rate_limit followed by ok recovers cleanly -- rate-limit-U-001")
{
    auto script       = std::filesystem::temp_directory_path() / "att_tmux_rl_u001.sh";
    auto counter_file = std::filesystem::temp_directory_path() / "att_tmux_rl_u001_count";
    auto ndl_file     = std::filesystem::temp_directory_path() / "att_tmux_rl_u001_ndl";
    TmpFile g_script{script};
    TmpFile g_counter{counter_file};
    TmpFile g_ndl{ndl_file};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_rl_u001"};

    // new-window writes rate_limit; second Enter (retry restart) writes ok.
    write_script(script, make_rate_limit_script(
        ndl_file.string(), counter_file.string(),
        R"json({"status":"error","error_type":"rate_limit","message":"session limit resets 12:00am (UTC)"})json",
        2,
        "restarted",
        R"json({"status":"ok","message":"recovered"})json"));

    std::chrono::seconds slept{0};
    ClaudeCodeTmuxBackend backend{script.string(), logs_root.path, 0, 0,
                                  [&slept](std::chrono::seconds d) { slept = d; }};
    Node node{};
    node.id = NodeId{"n_rl1"};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "recovered");
    SNITCH_CHECK(slept.count() > 0);
}

SNITCH_TEST_CASE("[claude_tmux] rate_limit exhausts retries and returns fail -- rate-limit-U-002")
{
    auto script       = std::filesystem::temp_directory_path() / "att_tmux_rl_u002.sh";
    auto counter_file = std::filesystem::temp_directory_path() / "att_tmux_rl_u002_count";
    auto ndl_file     = std::filesystem::temp_directory_path() / "att_tmux_rl_u002_ndl";
    TmpFile g_script{script};
    TmpFile g_counter{counter_file};
    TmpFile g_ndl{ndl_file};
    TmpDir  logs_root{std::filesystem::temp_directory_path() / "att_logs_rl_u002"};

    // Both the initial and retry done.json report rate_limit — retries exhausted.
    write_script(script, make_rate_limit_script(
        ndl_file.string(), counter_file.string(),
        R"json({"status":"error","error_type":"rate_limit","message":"still limited"})json",
        2,
        "restarted",
        R"json({"status":"error","error_type":"rate_limit","message":"still limited"})json"));

    ClaudeCodeTmuxBackend backend{script.string(), logs_root.path, 0, 0,
                                  [](std::chrono::seconds) {}};
    Node node{};
    node.id = NodeId{"n_rl2"};
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(result.error().status == StageStatus::fail);
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("rate_limit") != std::string::npos);
}
