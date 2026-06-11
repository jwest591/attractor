#include <attractor/checkpoint.hpp>

#include <attractor/types.hpp>
#include <chrono>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <type_safe/strong_typedef.hpp>

namespace attractor {

namespace {

auto now_iso8601() -> std::string
{
    const auto now = std::chrono::system_clock::now();
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(now));
}

}  // namespace

auto save_checkpoint(const LogsRoot& logs_root, const CheckpointData& data)
    -> std::expected<void, std::string>
{
    try {
        const std::filesystem::path dir{type_safe::get(logs_root)};
        std::filesystem::create_directories(dir);

        nlohmann::json j;
        j["timestamp"] = data.timestamp.empty() ? now_iso8601() : data.timestamp;
        j["current_node"] = type_safe::get(data.current_node);

        j["completed_nodes"] = nlohmann::json::array();
        for (const auto& id : data.completed_nodes) {
            j["completed_nodes"].push_back(type_safe::get(id));
        }

        j["node_retries"] = data.node_retries;
        j["context"] = data.context;
        j["logs"] = data.logs;

        const auto final_path = dir / "checkpoint.json";
        const auto temp_path = dir / "checkpoint.json.tmp";

        {
            std::ofstream out{temp_path};
            if (!out) {
                return std::unexpected{"save_checkpoint: cannot open " + temp_path.string()};
            }
            out << j.dump(2);
            out.flush();
            if (!out) {
                return std::unexpected{"save_checkpoint: write failed for " + temp_path.string()};
            }
        }

        try {
            std::filesystem::rename(temp_path, final_path);
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::filesystem::remove(temp_path);
            return std::unexpected{std::string{"save_checkpoint: rename failed: "} + e.what()};
        }
        return {};
    }
    catch (const std::exception& e) {
        return std::unexpected{std::string{"save_checkpoint: "} + e.what()};
    }
}

auto load_checkpoint(const LogsRoot& logs_root) -> std::expected<CheckpointData, std::string>
{
    try {
        const std::filesystem::path path{std::filesystem::path{type_safe::get(logs_root)} / "checkpoint.json"};

        if (!std::filesystem::exists(path)) {
            return std::unexpected{"load_checkpoint: not found: " + path.string()};
        }

        std::ifstream in{path};
        if (!in) {
            return std::unexpected{"load_checkpoint: cannot open " + path.string()};
        }

        const nlohmann::json j = nlohmann::json::parse(in);

        CheckpointData data;
        data.timestamp = j.value("timestamp", std::string{});
        data.current_node = NodeId{j.value("current_node", std::string{})};

        if (j.contains("completed_nodes") && j["completed_nodes"].is_array()) {
            for (const auto& id : j["completed_nodes"]) {
                data.completed_nodes.emplace_back(NodeId{id.get<std::string>()});
            }
        }

        if (j.contains("node_retries") && j["node_retries"].is_object()) {
            data.node_retries = j["node_retries"].get<std::map<std::string, int>>();
        }

        if (j.contains("context") && j["context"].is_object()) {
            data.context = j["context"];
        }

        if (j.contains("logs") && j["logs"].is_array()) {
            for (const auto& entry : j["logs"]) {
                data.logs.push_back(entry.get<std::string>());
            }
        }

        return data;
    }
    catch (const std::exception& e) {
        return std::unexpected{std::string{"load_checkpoint: "} + e.what()};
    }
}

}  // namespace attractor
