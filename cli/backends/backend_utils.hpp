#ifndef ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP
#define ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP

#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <regex>
#include <string>
#include <utility>

namespace attractor {

[[nodiscard]] inline auto derive_node_log_dir(const std::filesystem::path& logs_root,
                                              const NodeId& node_id,
                                              int counter) -> std::filesystem::path
{
    return logs_root / std::format("{:03d}-{}", counter, type_safe::get(node_id));
}

[[nodiscard]] inline std::string resolve_scripts_dir()
{
    if (const char* env = std::getenv("ATTRACTOR_SCRIPTS_DIR"); env && env[0] != '\0') {
        return env;
    }
    std::error_code ec;
    const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        const auto candidate = exe.parent_path() / "../share/attractor/scripts";
        std::error_code ec2;
        if (std::filesystem::is_directory(candidate, ec2)) {
            auto canonical = std::filesystem::canonical(candidate, ec2);
            if (!ec2) {
                return canonical.string();
            }
        }
    }
    return std::string{ATTRACTOR_CLI_SCRIPTS_DIR};
}

// Parses "resets H:MMam (Timezone)" from a rate-limit message.
// Returns {time_str, tz_str} or nullopt if not present.
[[nodiscard]] inline auto parse_rate_limit_reset(const std::string& msg)
    -> std::optional<std::pair<std::string, std::string>>
{
    static const std::regex k_re{R"(resets (\d+:\d+(?:am|pm)) \(([^)]+)\))",
                                 std::regex_constants::icase};
    std::smatch m;
    if (!std::regex_search(msg, m, k_re)) {
        return std::nullopt;
    }
    return std::make_pair(m[1].str(), m[2].str());
}

} // namespace attractor

#endif // ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP
