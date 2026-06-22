#ifndef ATTRACTOR_CLI_CLI_UTILS_HPP
#define ATTRACTOR_CLI_CLI_UTILS_HPP

#include <attractor/events.hpp>
#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <nlohmann/json.hpp>
#include <type_safe/strong_typedef.hpp>

#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace attractor {

// "YYYY-MM-DDTHH:MM:SSZ\0" = 21 bytes; 22 covers both ISO-8601 formats used here
constexpr std::size_t k_iso8601_buf_size = 22;

inline void render_event(const Event& event)
{
    std::visit(
        [](auto&& ev) {
            using T = std::decay_t<decltype(ev)>;
            if constexpr (std::is_same_v<T, StageStarted>) {
                std::cout << "[stage " << (ev.index + 1) << "] started: "
                          << type_safe::get(ev.id) << "\n";
            }
            else if constexpr (std::is_same_v<T, StageCompleted>) {
                std::cout << "[stage " << (ev.index + 1) << "] completed: "
                          << type_safe::get(ev.id) << "\n";
            }
            else {
                static_assert(std::is_same_v<T, void>,
                    "render_event: unhandled Event variant -- update this visitor");
            }
        },
        event);
}

[[nodiscard]] inline std::string generate_run_id()
{
    const auto now = std::chrono::system_clock::now();
    const auto tt  = std::chrono::system_clock::to_time_t(now);
    std::array<char, k_iso8601_buf_size> buf{};
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    auto* const tm_ptr = std::gmtime(&tt);
    if (tm_ptr == nullptr) {
        return std::string{"00000000T000000Z-"} + std::to_string(static_cast<unsigned>(::getpid()));
    }
    const auto written = std::strftime(buf.data(), buf.size(), "%Y%m%dT%H%M%SZ", tm_ptr);
    if (written == 0) {
        buf[0] = '\0';
    }
    return std::string{buf.data()} + "-" + std::to_string(static_cast<unsigned>(::getpid()));
}

[[nodiscard]] inline bool write_manifest(const std::string& logs_root_str,
                                         const std::string& run_id,
                                         const Graph& graph)
{
    namespace fs = std::filesystem;
    const fs::path dir{logs_root_str};
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        return false;
    }
    std::ofstream f{dir / "manifest.json"};
    if (!f) {
        return false;
    }
    nlohmann::json j;
    j["run_id"]     = run_id;
    j["graph_id"]   = type_safe::get(graph.digraph_id);
    j["node_count"] = graph.nodes.size();
    j["edge_count"] = graph.edges.size();
    const auto now  = std::chrono::system_clock::now();
    const auto tt   = std::chrono::system_clock::to_time_t(now);
    std::array<char, k_iso8601_buf_size> buf{};
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    auto* const tm_ptr = std::gmtime(&tt);
    if (tm_ptr != nullptr) {
        const auto written =
            std::strftime(buf.data(), buf.size(), "%Y-%m-%dT%H:%M:%SZ", tm_ptr);
        if (written == 0) {
            buf[0] = '\0';
        }
    }
    j["run_started"] = tm_ptr != nullptr ? buf.data() : "1970-01-01T00:00:00Z";
    f << j.dump(2) << "\n";
    return f.good();
}

}  // namespace attractor

#endif  // ATTRACTOR_CLI_CLI_UTILS_HPP
