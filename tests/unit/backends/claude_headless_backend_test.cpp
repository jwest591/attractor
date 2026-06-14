#include "claude_headless_backend.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <snitch/snitch.hpp>
#include <type_safe/strong_typedef.hpp>
#include <filesystem>
#include <fstream>

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
             "'{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"}}'\n";
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
             "'{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"}}'\n";
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
