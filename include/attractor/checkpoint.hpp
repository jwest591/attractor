#ifndef ATTRACTOR_CHECKPOINT_HPP
#define ATTRACTOR_CHECKPOINT_HPP

#include <attractor/types.hpp>
#include <expected>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace attractor {

struct CheckpointData {
    std::string timestamp;
    NodeId current_node;
    std::vector<NodeId> completed_nodes;
    std::map<std::string, int> node_retries;
    nlohmann::json context = nlohmann::json::object();
    std::vector<std::string> logs;
};

// Write checkpoint atomically to {logs_root}/checkpoint.json (temp + rename).
[[nodiscard]] auto save_checkpoint(const LogsRoot& logs_root, const CheckpointData& data)
    -> std::expected<void, std::string>;

// Read and parse {logs_root}/checkpoint.json.
[[nodiscard]] auto load_checkpoint(const LogsRoot& logs_root)
    -> std::expected<CheckpointData, std::string>;

}  // namespace attractor

#endif  // ATTRACTOR_CHECKPOINT_HPP
