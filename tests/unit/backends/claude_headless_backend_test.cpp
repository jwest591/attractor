#include "claude_headless_backend.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <snitch/snitch.hpp>
#include <type_safe/strong_typedef.hpp>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace attractor;

namespace {
// Fix: RAII guard so temp files are removed even when SNITCH_REQUIRE aborts the test
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions) -- move intentionally omitted; guard is always local
struct TmpFile {
    explicit TmpFile(std::filesystem::path p) : path{std::move(p)} {}
    TmpFile(const TmpFile&) = delete;
    TmpFile& operator=(const TmpFile&) = delete;
    ~TmpFile() noexcept { std::error_code ec; std::filesystem::remove(path, ec); }
    std::filesystem::path path;
};

// RAII guard that unsets an environment variable on scope exit.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions) -- move intentionally omitted; guard is always local
struct EnvGuard {
    explicit EnvGuard(const char* name) : m_name{name} {}
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;
    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- test binary is single-threaded; env mutation is test-local
    ~EnvGuard() noexcept { unsetenv(m_name); }
    const char* m_name;
};
}  // namespace

SNITCH_TEST_CASE("[claude_headless] stream-json end_turn assembled as LlmResponse -- 5.2-U-001")
{
    auto tmp = std::filesystem::temp_directory_path() / "att_test_echo.sh";
    TmpFile guard{tmp};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\n"
             "printf '%s\\n' "
             "'{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"hello world\"}}' "
             "'{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"}}' "
             "'{\"type\":\"result\",\"is_error\":false,\"result\":\"hello world\",\"stop_reason\":\"end_turn\"}'\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    ClaudeCodeHeadlessBackend echo_backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto result = echo_backend.run(node, PromptText{"hello world"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "hello world");
}

SNITCH_TEST_CASE("[claude_headless] non-zero exit returns FAIL with stderr content -- 5.2-U-002")
{
    auto tmp = std::filesystem::temp_directory_path() / "att_test_fail.sh";
    TmpFile guard{tmp};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\necho 'something went wrong' >&2\nexit 1\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    ClaudeCodeHeadlessBackend fail_backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto result = fail_backend.run(node, PromptText{"prompt"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("something went wrong") != std::string::npos);
}

SNITCH_TEST_CASE("[claude_headless] timeout kills subprocess and returns FAIL -- 5.2-U-003")
{
    auto tmp = std::filesystem::temp_directory_path() / "att_test_sleep.sh";
    TmpFile guard{tmp};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\nsleep 60\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    ClaudeCodeHeadlessBackend sleep_backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    node.timeout = TimeoutDuration{std::chrono::milliseconds{100}};
    Context ctx;

    auto result = sleep_backend.run(node, PromptText{""}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(type_safe::get(result.error().failure_reason) == "timeout");
    SNITCH_CHECK(result.error().status == StageStatus::fail);
}

SNITCH_TEST_CASE("[claude_headless] stream-json text_delta events assembled into LlmResponse -- 5.4-U-006")
{
    auto tmp = std::filesystem::temp_directory_path() / "att_test_stream_006.sh";
    TmpFile guard{tmp};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\n"
             "printf '%s\\n' "
             "'{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"hello \"}}' "
             "'{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"world\"}}' "
             "'{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"}}' "
             "'{\"type\":\"result\",\"is_error\":false,\"result\":\"hello world\",\"stop_reason\":\"end_turn\"}'\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    ClaudeCodeHeadlessBackend backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto result = backend.run(node, PromptText{"prompt"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "hello world");
}

#ifdef ATTRACTOR_ENABLE_SLOW_TESTS
SNITCH_TEST_CASE("[claude_headless][slow] rate_limit_error exhausts retries and returns FAIL -- 5.4-U-007")
{
    auto tmp = std::filesystem::temp_directory_path() / "att_test_ratelimit_007.sh";
    TmpFile guard{tmp};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\n"
             "printf '%s\\n' "
             "'{\"type\":\"error\",\"error\":{\"type\":\"rate_limit_error\",\"message\":\"Too Many Requests\"}}'\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    ClaudeCodeHeadlessBackend backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto result = backend.run(node, PromptText{"prompt"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("rate_limit_error") != std::string::npos);
}
#endif  // ATTRACTOR_ENABLE_SLOW_TESTS

SNITCH_TEST_CASE("[claude_headless] non-rate-limit API error returns FAIL immediately -- 5.4-U-008")
{
    auto tmp = std::filesystem::temp_directory_path() / "att_test_apierr_008.sh";
    TmpFile guard{tmp};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\n"
             "printf '%s\\n' "
             "'{\"type\":\"error\",\"error\":{\"type\":\"overloaded_error\",\"message\":\"Service overloaded\"}}'\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    ClaudeCodeHeadlessBackend backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto result = backend.run(node, PromptText{"prompt"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("overloaded_error") != std::string::npos);
}

SNITCH_TEST_CASE("[claude_headless] parse-stream interposed: usage file written for session -- 5.6-U-001")
{
    auto tmp_sh    = std::filesystem::temp_directory_path() / "att_test_usage_001.sh";
    auto tmp_usage = std::filesystem::temp_directory_path() / "att_test_ctx_usage_001.json";
    TmpFile guard_sh{tmp_sh};
    TmpFile guard_usage{tmp_usage};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};
    {
        std::ofstream f{tmp_sh};
        f << "#!/bin/sh\n"
             "printf '%s\\n' "
             "'{\"type\":\"message_start\",\"session_id\":\"test-session-001\","
             "\"usage\":{\"input_tokens\":1000,\"output_tokens\":200}}' "
             "'{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"hello\"}}' "
             "'{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"}}' "
             "'{\"type\":\"result\",\"is_error\":false,\"result\":\"hello\",\"stop_reason\":\"end_turn\"}'\n";
    }
    std::filesystem::permissions(tmp_sh, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    setenv("CLAUDE_USAGE_FILE", tmp_usage.string().c_str(), 1);
    EnvGuard env_guard{"CLAUDE_USAGE_FILE"};

    ClaudeCodeHeadlessBackend backend{tmp_sh.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "hello");
    SNITCH_CHECK(std::filesystem::exists(tmp_usage));
}

SNITCH_TEST_CASE("[claude_headless] fd-aliasing: pipe fd 1 renumbered before dup2 -- 7.3-U-001")
{
    auto tmp = std::filesystem::temp_directory_path() / "att_test_renumber_001.sh";
    TmpFile guard{tmp};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\n"
             "printf '%s\\n' "
             "'{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"renumber ok\"}}' "
             "'{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"}}' "
             "'{\"type\":\"result\",\"is_error\":false,\"result\":\"renumber ok\",\"stop_reason\":\"end_turn\"}'\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    int saved_stdout = dup(STDOUT_FILENO);
    SNITCH_REQUIRE(saved_stdout >= 0);
    close(STDOUT_FILENO);

    ClaudeCodeHeadlessBackend backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;
    auto result = backend.run(node, PromptText{"hello"}, ctx);

    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "renumber ok");
}

SNITCH_TEST_CASE("[claude_headless] dup2 failure path: exit code 126 returns FAIL with stderr -- 7.3-U-002")
{
    auto tmp = std::filesystem::temp_directory_path() / "att_test_exit126_002.sh";
    TmpFile guard{tmp};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\necho 'dup2 setup failed' >&2\nexit 126\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    ClaudeCodeHeadlessBackend backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;
    auto result = backend.run(node, PromptText{"prompt"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(type_safe::get(result.error().failure_reason).find("dup2 setup failed") != std::string::npos);
}

SNITCH_TEST_CASE("[claude_headless] fd sweep: child does not inherit non-CLOEXEC parent fd -- 7.3-U-003")
{
    int extra_fd = open("/dev/null", O_RDONLY);
    SNITCH_REQUIRE(extra_fd >= 0);
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    setenv("ATTRACTOR_TEST_EXTRA_FD", std::to_string(extra_fd).c_str(), 1);
    EnvGuard env_guard{"ATTRACTOR_TEST_EXTRA_FD"};

    auto tmp = std::filesystem::temp_directory_path() / "att_test_fdsweep_003.sh";
    TmpFile guard{tmp};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\n"
             "fd=${ATTRACTOR_TEST_EXTRA_FD:-}\n"
             "if [ -n \"$fd\" ] && [ -e \"/proc/self/fd/$fd\" ]; then r=inherited; else r=closed; fi\n"
             "printf '{\"type\":\"result\",\"is_error\":false,\"result\":\"%s\",\"stop_reason\":\"end_turn\"}\\n' \"$r\"\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    ClaudeCodeHeadlessBackend backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;
    auto result = backend.run(node, PromptText{"hello"}, ctx);

    close(extra_fd);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "closed");
}

SNITCH_TEST_CASE("[claude_headless] RAII: UniqueFd closes all pipe fds after subprocess -- 7.3-U-004")
{
    auto count_fds = []() -> int {
        DIR* d = opendir("/proc/self/fd");
        if (d == nullptr) return -1;
        int n = 0;
        while (const auto* ent = readdir(d)) {
            if (ent->d_name[0] != '.') ++n;
        }
        closedir(d);
        return n;
    };

    auto tmp = std::filesystem::temp_directory_path() / "att_test_raii_004.sh";
    TmpFile guard{tmp};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\n"
             "printf '%s\\n' "
             "'{\"type\":\"result\",\"is_error\":false,\"result\":\"ok\",\"stop_reason\":\"end_turn\"}'\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    int fds_before = count_fds();
    SNITCH_REQUIRE(fds_before >= 0);

    ClaudeCodeHeadlessBackend backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;
    auto result = backend.run(node, PromptText{"hello"}, ctx);

    int fds_after = count_fds();
    SNITCH_REQUIRE(fds_after >= 0);
    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(fds_after == fds_before);
}

SNITCH_TEST_CASE("[claude_headless] parse-stream passthrough: multi-delta text assembled with usage fields present -- 5.6-U-002")
{
    auto tmp_sh    = std::filesystem::temp_directory_path() / "att_test_passthrough_002.sh";
    auto tmp_usage = std::filesystem::temp_directory_path() / "att_test_ctx_usage_002.json";
    TmpFile guard_sh{tmp_sh};
    TmpFile guard_usage{tmp_usage};
    TmpFile g_handoff{std::filesystem::current_path() / ".attractor" / "att-n1-handoff.md"};
    {
        std::ofstream f{tmp_sh};
        f << "#!/bin/sh\n"
             "printf '%s\\n' "
             "'{\"type\":\"message_start\",\"session_id\":\"test-session-002\","
             "\"usage\":{\"input_tokens\":500,\"output_tokens\":100}}' "
             "'{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"part1 \"}}' "
             "'{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"part2\"}}' "
             "'{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"}}' "
             "'{\"type\":\"result\",\"is_error\":false,\"result\":\"part1 part2\",\"stop_reason\":\"end_turn\"}'\n";
    }
    std::filesystem::permissions(tmp_sh, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    setenv("CLAUDE_USAGE_FILE", tmp_usage.string().c_str(), 1);
    EnvGuard env_guard{"CLAUDE_USAGE_FILE"};

    ClaudeCodeHeadlessBackend backend{tmp_sh.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;

    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "part1 part2");
    SNITCH_CHECK(std::filesystem::exists(tmp_usage));
}
