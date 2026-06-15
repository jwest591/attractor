#include "backend_utils.hpp"

#include <snitch/snitch.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace attractor;

SNITCH_TEST_CASE("[backend_utils] compute_transcript_marker_path returns path under .attractor/ not /tmp/ -- 5.7-U-001")
{
    auto result = compute_transcript_marker_path("att-n1");

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(result->find(".attractor") != std::string::npos);
    SNITCH_CHECK(result->find("/tmp/") == std::string::npos);
}

SNITCH_TEST_CASE("[backend_utils] compute_transcript_marker_path returns cwd/.attractor/att-<name>-transcript.txt -- 5.7-U-002")
{
    const auto expected =
        (std::filesystem::current_path() / ".attractor" / "att-att-n1-transcript.txt").string();

    auto result = compute_transcript_marker_path("att-n1");

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(*result == expected);
}

SNITCH_TEST_CASE("[backend_utils] compute_transcript_marker_path creates .attractor/ directory if absent -- 5.7-U-003")
{
    auto result = compute_transcript_marker_path("att-n1");

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(std::filesystem::is_directory(std::filesystem::current_path() / ".attractor"));
}

SNITCH_TEST_CASE("[backend_utils] compute_transcript_marker_path returns unexpected when directory creation fails -- 5.7-U-004")
{
    const auto dir = std::filesystem::current_path() / ".attractor";
    std::error_code ec;

    // Block directory creation by placing a regular file at the .attractor path.
    std::filesystem::remove_all(dir, ec);
    { std::ofstream blocker{dir}; }

    // RAII: restore .attractor/ as a directory after the test regardless of outcome.
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    struct RestoreDir {
        std::filesystem::path p;
        ~RestoreDir() noexcept {
            std::error_code e;
            std::filesystem::remove(p, e);
            std::filesystem::create_directories(p, e);
        }
    } restore{dir};

    auto result = compute_transcript_marker_path("att-n1");

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(!result.error().empty());
}
