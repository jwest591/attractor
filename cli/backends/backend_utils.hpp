#ifndef ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP
#define ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP

#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

#include <filesystem>
#include <string>

namespace attractor {

[[nodiscard]] inline auto derive_node_log_dir(const std::filesystem::path& logs_root,
                                              const NodeId& node_id,
                                              int counter) -> std::filesystem::path
{
    return logs_root / (type_safe::get(node_id) + "-" + std::to_string(counter));
}

} // namespace attractor

#endif // ATTRACTOR_CLI_BACKENDS_BACKEND_UTILS_HPP
