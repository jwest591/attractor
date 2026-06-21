// Tests for att-session-start.sh writing transcript.txt to ATTRACTOR_NODE_LOG_DIR (Story 7.19).
#include <snitch/snitch.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

const char* k_script = ATTRACTOR_CLI_SCRIPTS_DIR "/att-session-start.sh";

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

int invoke_script(const std::string& node_log_dir, const std::string& transcript_path)
{
    const std::string json =
        "{\"transcript_path\":\"" + transcript_path + "\"}";
    const std::string cmd =
        "printf '%s\\n' '" + json + "' | "
        "ATTRACTOR_NODE_LOG_DIR='" + node_log_dir + "' " + k_script;
    return std::system(cmd.c_str()); // NOLINT(cert-env33-c)
}

}  // namespace

SNITCH_TEST_CASE("[att_session_start] writes transcript path to ATTRACTOR_NODE_LOG_DIR/transcript.txt -- 7.19-I-001")
{
    TmpDir nld{std::filesystem::temp_directory_path() / "att_ss_7_19_i001"};
    const std::string transcript = "/home/user/.claude/projects/foo/bar.jsonl";

    const auto expected = nld.path / "transcript.txt";

    const int rc = invoke_script(nld.path.string(), transcript);
    SNITCH_CHECK(rc == 0);
    SNITCH_REQUIRE(std::filesystem::exists(expected));

    std::ifstream f{expected};
    std::string contents;
    std::getline(f, contents);
    SNITCH_CHECK(contents == transcript);
}

SNITCH_TEST_CASE("[att_session_start] fails when ATTRACTOR_NODE_LOG_DIR is unset -- 7.19-I-002")
{
    const std::string json = "{\"transcript_path\":\"/tmp/test.jsonl\"}";
    const std::string cmd = "printf '%s\\n' '" + json + "' | " + std::string{k_script};
    const int rc = std::system(cmd.c_str()); // NOLINT(cert-env33-c)
    SNITCH_CHECK(rc != 0);
}

SNITCH_TEST_CASE("[att_session_start] fails when transcript.txt already exists -- 7.19-I-003")
{
    TmpDir nld{std::filesystem::temp_directory_path() / "att_ss_7_19_i003"};
    const auto existing = nld.path / "transcript.txt";
    { std::ofstream f{existing}; f << "old content\n"; }

    const int rc = invoke_script(nld.path.string(), "/tmp/new.jsonl");
    SNITCH_CHECK(rc != 0);

    // Existing content must be preserved
    std::ifstream f{existing};
    std::string contents;
    std::getline(f, contents);
    SNITCH_CHECK(contents == "old content");
}
