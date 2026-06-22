#ifndef ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP
#define ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP

#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

namespace attractor {

[[nodiscard]] inline auto derive_node_log_dir(const std::filesystem::path& logs_root,
                                              const NodeId& node_id,
                                              int counter) -> std::filesystem::path
{
    return logs_root / (type_safe::get(node_id) + "-" + std::to_string(counter));
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

} // namespace attractor

#endif // ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP
