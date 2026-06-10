#ifndef ATTRACTOR_TESTS_SUPPORT_ATTRACTOR_TEST_SUPPORT_HPP
#define ATTRACTOR_TESTS_SUPPORT_ATTRACTOR_TEST_SUPPORT_HPP

#include <snitch/snitch.hpp>

#include <atomic>
#include <attractor/dot_parser.hpp>
#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <filesystem>
#include <string>

namespace attractor::test {

// Parse DOT source; fails the test immediately if parsing fails.
// Call only inside a SNITCH_TEST_CASE.
inline auto parse_ok(std::string_view dot) -> Graph
{
    auto result = attractor::parse_graph(dot);
    SNITCH_REQUIRE(result.has_value());
    return std::move(*result);
}

// RAII temporary directory for engine test runs.
// Automatically removed on destruction.
class TempLogsDir {
  public:
    TempLogsDir()
    {
        static std::atomic<int> k_counter{0};
        m_path = std::filesystem::temp_directory_path() / ("att_engine_test_" + std::to_string(++k_counter));
        std::filesystem::create_directories(m_path);
    }

    ~TempLogsDir() { std::filesystem::remove_all(m_path); }

    TempLogsDir(const TempLogsDir&) = delete;
    TempLogsDir& operator=(const TempLogsDir&) = delete;
    TempLogsDir(TempLogsDir&&) = delete;
    TempLogsDir& operator=(TempLogsDir&&) = delete;

    [[nodiscard]] auto logs_root() const -> LogsRoot { return LogsRoot{m_path.string()}; }

    [[nodiscard]] auto path() const -> const std::filesystem::path& { return m_path; }

  private:
    std::filesystem::path m_path;
};

}  // namespace attractor::test

#endif  // ATTRACTOR_TESTS_SUPPORT_ATTRACTOR_TEST_SUPPORT_HPP
