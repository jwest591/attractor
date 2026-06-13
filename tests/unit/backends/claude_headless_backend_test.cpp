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

SNITCH_TEST_CASE("[claude_headless] stdout from subprocess returned as LlmResponse -- 5.2-U-001")
{
    auto tmp = std::filesystem::temp_directory_path() / "att_test_echo.sh";
    TmpFile guard{tmp};
    {
        std::ofstream f{tmp};
        f << "#!/bin/sh\ncat\n";
    }
    std::filesystem::permissions(tmp, std::filesystem::perms::owner_exec
        | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);

    ClaudeCodeHeadlessBackend echo_backend{tmp.string()};
    Node node;
    node.id = NodeId{"n1"};
    Context ctx;
    const PromptText prompt{"hello world"};

    auto result = echo_backend.run(node, prompt, ctx);

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
