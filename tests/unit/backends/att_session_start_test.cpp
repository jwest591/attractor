// Tests for att-session-start.sh writing the transcript marker to .attractor/ (Story 5.7).
#include <snitch/snitch.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

const char* k_script = ATTRACTOR_CLI_SCRIPTS_DIR "/att-session-start.sh";

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

int invoke_script(const std::string& project_dir, const std::string& session_title,
                  const std::string& transcript_path)
{
    const std::string json =
        "{\"session_title\":\"" + session_title + "\","
        "\"transcript_path\":\"" + transcript_path + "\"}";
    const std::string cmd =
        "printf '%s\\n' '" + json + "' | "
        "CLAUDE_PROJECT_DIR='" + project_dir + "' " + k_script;
    return std::system(cmd.c_str()); // NOLINT(cert-env33-c)
}

}  // namespace

SNITCH_TEST_CASE("[att_session_start] writes marker under .attractor/ not /tmp/ -- 5.7-I-001")
{
    TmpDir proj{std::filesystem::temp_directory_path() / "att_57_i001"};
    const std::string title = "att-test57a";
    const std::string transcript = "/tmp/att-att-test57a-test.jsonl";

    const auto expected = (proj.path / ".attractor" / ("att-" + title + "-transcript.txt")).string();
    const std::string legacy = "/tmp/att-" + title + "-transcript.txt";

    std::error_code ec;
    std::filesystem::remove(legacy, ec);

    const int rc = invoke_script(proj.path.string(), title, transcript);
    SNITCH_CHECK(rc == 0);

    // AC#1: new path must exist; /tmp/ path must not
    SNITCH_CHECK(std::filesystem::exists(expected));
    SNITCH_CHECK_FALSE(std::filesystem::exists(legacy));
}

SNITCH_TEST_CASE("[att_session_start] creates .attractor/ directory when absent -- 5.7-I-002")
{
    TmpDir proj{std::filesystem::temp_directory_path() / "att_57_i002"};
    const std::string title = "att-test57b";
    const std::string transcript = "/tmp/att-att-test57b-test.jsonl";

    SNITCH_REQUIRE_FALSE(std::filesystem::exists(proj.path / ".attractor"));

    SNITCH_CHECK(invoke_script(proj.path.string(), title, transcript) == 0);

    SNITCH_CHECK(std::filesystem::is_directory(proj.path / ".attractor"));
}
