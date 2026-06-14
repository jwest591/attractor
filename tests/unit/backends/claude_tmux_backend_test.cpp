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

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions) -- copy disabled; move not needed for RAII test helper
struct TmpFile {
    explicit TmpFile(std::filesystem::path p) : path{std::move(p)} {
        std::error_code ec;
        std::filesystem::remove(path, ec);  // remove stale file from a prior crashed run
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

}  // namespace

// Session name derived from NodeId{"n1"} with no thread_id: "att-n1"
// Marker file:   /tmp/att-att-n1-transcript.txt
// JSONL file:    /tmp/att-att-n1-test.jsonl  (written by mock new-session)

SNITCH_TEST_CASE("[claude_tmux] first run creates session and returns LlmResponse -- 5.3-U-001")
{
    auto script = std::filesystem::temp_directory_path() / "att_test_tmux_001.sh";
    TmpFile g_script{script};
    TmpFile g_marker{std::filesystem::path{"/tmp/att-att-n1-transcript.txt"}};
    TmpFile g_jsonl{std::filesystem::path{"/tmp/att-att-n1-test.jsonl"}};

    write_script(script, R"(#!/bin/sh
case "$1" in
  has-session) exit 1 ;;
  new-session)
    NAME="$4"
    JSONL="/tmp/att-${NAME}-test.jsonl"
    touch "$JSONL"
    printf '%s\n' "$JSONL" > "/tmp/att-${NAME}-transcript.txt"
    exit 0 ;;
  send-keys)
    NAME="$3"
    JSONL="/tmp/att-${NAME}-test.jsonl"
    printf '{"type":"assistant","message":{"role":"assistant","content":[{"type":"text","text":"mock response"}],"stop_reason":"end_turn"}}\n' >> "$JSONL"
    exit 0 ;;
esac
exit 1
)");

    ClaudeCodeTmuxBackend backend{script.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "mock response");
}

SNITCH_TEST_CASE("[claude_tmux] second run reuses cached session without new-session call -- 5.3-U-002")
{
    auto script = std::filesystem::temp_directory_path() / "att_test_tmux_002.sh";
    auto state  = std::filesystem::temp_directory_path() / "att_test_tmux_002_state.txt";
    TmpFile g_script{script};
    TmpFile g_state{state};
    TmpFile g_marker{std::filesystem::path{"/tmp/att-att-n1-transcript.txt"}};
    TmpFile g_jsonl{std::filesystem::path{"/tmp/att-att-n1-test.jsonl"}};

    // Embed state file path so mock can record new-session invocations
    write_script(script,
        "#!/bin/sh\n"
        "STATE=\"" + state.string() + "\"\n"
        "case \"$1\" in\n"
        "  has-session) exit 1 ;;\n"
        "  new-session)\n"
        "    NAME=\"$4\"\n"
        "    JSONL=\"/tmp/att-${NAME}-test.jsonl\"\n"
        "    touch \"$JSONL\"\n"
        "    printf '%s\\n' \"$JSONL\" > \"/tmp/att-${NAME}-transcript.txt\"\n"
        "    printf 'new-session\\n' >> \"$STATE\"\n"
        "    exit 0 ;;\n"
        "  send-keys)\n"
        "    NAME=\"$3\"\n"
        "    JSONL=\"/tmp/att-${NAME}-test.jsonl\"\n"
        "    printf '{\"type\":\"assistant\",\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"mock response\"}],\"stop_reason\":\"end_turn\"}}\\n' >> \"$JSONL\"\n"
        "    exit 0 ;;\n"
        "esac\n"
        "exit 1\n");

    ClaudeCodeTmuxBackend backend{script.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto r1 = backend.run(node, PromptText{"first"}, ctx);
    SNITCH_REQUIRE(r1.has_value());

    auto r2 = backend.run(node, PromptText{"second"}, ctx);
    SNITCH_REQUIRE(r2.has_value());

    // new-session must have been called exactly once (second run is a cache hit)
    std::ifstream sf{state.string()};
    int new_session_calls = 0;
    std::string line;
    while (std::getline(sf, line)) { ++new_session_calls; }
    SNITCH_CHECK(new_session_calls == 1);
}

SNITCH_TEST_CASE("[claude_tmux] session creation failure returns FAIL outcome -- 5.3-U-003")
{
    auto script = std::filesystem::temp_directory_path() / "att_test_tmux_003.sh";
    TmpFile g_script{script};

    write_script(script, R"(#!/bin/sh
case "$1" in
  has-session) exit 1 ;;
  new-session)  exit 1 ;;
esac
exit 1
)");

    ClaudeCodeTmuxBackend backend{script.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto result = backend.run(node, PromptText{"prompt"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("failed to obtain transcript") != std::string::npos);
}

SNITCH_TEST_CASE("[claude_tmux] timeout waiting for end_turn returns FAIL outcome -- 5.3-U-004")
{
    auto script = std::filesystem::temp_directory_path() / "att_test_tmux_004.sh";
    TmpFile g_script{script};
    TmpFile g_marker{std::filesystem::path{"/tmp/att-att-n1-transcript.txt"}};
    TmpFile g_jsonl{std::filesystem::path{"/tmp/att-att-n1-test.jsonl"}};

    // send-keys does NOT append an end_turn entry, forcing wait_for_end_turn to time out
    write_script(script, R"(#!/bin/sh
case "$1" in
  has-session) exit 1 ;;
  new-session)
    NAME="$4"
    JSONL="/tmp/att-${NAME}-test.jsonl"
    touch "$JSONL"
    printf '%s\n' "$JSONL" > "/tmp/att-${NAME}-transcript.txt"
    exit 0 ;;
  send-keys) exit 0 ;;
esac
exit 1
)");

    ClaudeCodeTmuxBackend backend{script.string()};
    Node node;
    node.id = NodeId{"n1"};
    node.timeout = TimeoutDuration{std::chrono::milliseconds{300}};
    Context ctx;

    auto result = backend.run(node, PromptText{"prompt"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(type_safe::get(result.error().failure_reason) == "timeout");
    SNITCH_CHECK(result.error().status == StageStatus::fail);
}

SNITCH_TEST_CASE("[claude_tmux] max_tokens response returns FAIL with max_tokens diagnostic -- 5.4-U-001")
{
    auto script = std::filesystem::temp_directory_path() / "att_test_tmux_54_001.sh";
    TmpFile g_script{script};
    TmpFile g_marker{std::filesystem::path{"/tmp/att-att-n1-transcript.txt"}};
    TmpFile g_jsonl{std::filesystem::path{"/tmp/att-att-n1-test.jsonl"}};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};

    write_script(script, R"(#!/bin/sh
case "$1" in
  has-session) exit 1 ;;
  new-session)
    NAME="$4"
    JSONL="/tmp/att-${NAME}-test.jsonl"
    touch "$JSONL"
    printf '%s\n' "$JSONL" > "/tmp/att-${NAME}-transcript.txt"
    exit 0 ;;
  send-keys)
    NAME="$3"
    JSONL="/tmp/att-${NAME}-test.jsonl"
    printf '{"type":"assistant","message":{"role":"assistant","content":[{"type":"text","text":"partial"}],"stop_reason":"max_tokens"}}\n' >> "$JSONL"
    exit 0 ;;
esac
exit 1
)");

    ClaudeCodeTmuxBackend backend{script.string()};
    Node node;
    node.id = NodeId{"n1"};
    node.timeout = TimeoutDuration{std::chrono::milliseconds{500}};
    Context ctx;

    auto result = backend.run(node, PromptText{"prompt"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("max_tokens") != std::string::npos);
}

SNITCH_TEST_CASE("[claude_tmux] rate_limit_error exhausts retries and returns FAIL -- 5.4-U-002")
{
    // The mock writes a local_command event with a past date when /usage is sent so
    // parse_reset_duration returns 0s; the node timeout is shorter than the 5s sleep
    // buffer so no actual sleep occurs between retries.
    auto script = std::filesystem::temp_directory_path() / "att_test_tmux_54_002.sh";
    TmpFile g_script{script};
    TmpFile g_marker{std::filesystem::path{"/tmp/att-att-n1-transcript.txt"}};
    TmpFile g_jsonl{std::filesystem::path{"/tmp/att-att-n1-test.jsonl"}};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};

    write_script(script, R"(#!/bin/sh
case "$1" in
  has-session) exit 1 ;;
  new-session)
    NAME="$4"
    JSONL="/tmp/att-${NAME}-test.jsonl"
    touch "$JSONL"
    printf '%s\n' "$JSONL" > "/tmp/att-${NAME}-transcript.txt"
    exit 0 ;;
  send-keys)
    NAME="$3"
    JSONL="/tmp/att-${NAME}-test.jsonl"
    if [ "$5" = "/usage" ]; then
      printf '{"type":"system","subtype":"local_command","content":"<local-command-stdout>Current session: 99%% used - resets 2000-01-01 00:00:00 (UTC)</local-command-stdout>"}\n' >> "$JSONL"
    else
      printf '{"type":"error","error":{"type":"rate_limit_error","message":"Too Many Requests"}}\n' >> "$JSONL"
    fi
    exit 0 ;;
esac
exit 1
)");

    ClaudeCodeTmuxBackend backend{script.string()};
    Node node;
    node.id = NodeId{"n1"};
    node.timeout = TimeoutDuration{std::chrono::milliseconds{2000}};
    Context ctx;

    auto result = backend.run(node, PromptText{"prompt"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("rate_limit_error") != std::string::npos);
}

SNITCH_TEST_CASE("[claude_tmux] end_turn with input_tokens does not corrupt happy path -- 5.4-U-003")
{
    auto script = std::filesystem::temp_directory_path() / "att_test_tmux_54_003.sh";
    TmpFile g_script{script};
    TmpFile g_marker{std::filesystem::path{"/tmp/att-att-n1-transcript.txt"}};
    TmpFile g_jsonl{std::filesystem::path{"/tmp/att-att-n1-test.jsonl"}};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};

    write_script(script, R"(#!/bin/sh
case "$1" in
  has-session) exit 1 ;;
  new-session)
    NAME="$4"
    JSONL="/tmp/att-${NAME}-test.jsonl"
    touch "$JSONL"
    printf '%s\n' "$JSONL" > "/tmp/att-${NAME}-transcript.txt"
    exit 0 ;;
  send-keys)
    NAME="$3"
    JSONL="/tmp/att-${NAME}-test.jsonl"
    printf '{"type":"assistant","message":{"role":"assistant","content":[{"type":"text","text":"mock response"}],"stop_reason":"end_turn","usage":{"input_tokens":42}}}\n' >> "$JSONL"
    exit 0 ;;
esac
exit 1
)");

    ClaudeCodeTmuxBackend backend{script.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "mock response");
}
