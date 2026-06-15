#ifndef ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP
#define ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP

#include <attractor/graph.hpp>
#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace attractor {

inline std::string derive_session_name(const Node& node)
{
    std::string raw = node.thread_id.has_value()
        ? type_safe::get(*node.thread_id)
        : type_safe::get(node.id);
    std::string name = "att-" + raw;
    for (char& c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '-';
    }
    return name;
}

[[nodiscard]] inline std::expected<std::string, std::string> compute_handoff_path(
    const std::string& session_name)
{
    const auto dir = std::filesystem::current_path() / ".attractor";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return std::unexpected(ec.message());
    return (dir / (session_name + "-handoff.md")).string();
}

[[nodiscard]] inline std::expected<std::string, std::string> compute_transcript_marker_path(
    const std::string& session_name)
{
    const auto dir = std::filesystem::current_path() / ".attractor";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return std::unexpected(ec.message());
    return (dir / ("att-" + session_name + "-transcript.txt")).string();
}

} // namespace attractor

#endif // ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP
