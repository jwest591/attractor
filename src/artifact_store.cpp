#include <attractor/artifact_store.hpp>

#include <attractor/types.hpp>
#include <expected>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <type_safe/strong_typedef.hpp>
#include <vector>

namespace attractor {

ArtifactStore::ArtifactStore(LogsRoot logs_root) : m_logs_root{std::move(logs_root)} {}

auto ArtifactStore::store(const ArtifactId& id, std::string name, nlohmann::json data)
    -> std::expected<ArtifactInfo, std::string>
{
    try {
        const std::string serialized = data.dump();
        const std::size_t size = serialized.size();
        const bool file_backed = (size >= k_artifact_file_threshold) && !m_logs_root.empty();

        ArtifactInfo info;
        info.id = id;
        info.name = std::move(name);
        info.size_bytes = size;
        info.is_file_backed = file_backed;

        Entry entry;
        entry.info = info;

        if (file_backed) {
            const std::filesystem::path dir = std::filesystem::path{type_safe::get(m_logs_root)} / "artifacts";
            std::filesystem::create_directories(dir);
            const auto file = dir / (type_safe::get(id) + ".json");
            {
                std::ofstream out{file};
                if (!out) {
                    return std::unexpected{"ArtifactStore::store: cannot open " + file.string()};
                }
                out << serialized;
                if (!out.flush()) {
                    return std::unexpected{"ArtifactStore::store: write failed for " + file.string()};
                }
            }
            entry.file_path = file;
        }
        else {
            entry.inline_data = std::move(data);
        }

        std::unique_lock lock{m_mutex};
        m_entries[id] = std::move(entry);
        return info;
    }
    catch (const std::exception& e) {
        return std::unexpected{std::string{"ArtifactStore::store: "} + e.what()};
    }
}

auto ArtifactStore::retrieve(const ArtifactId& id) const
    -> std::expected<nlohmann::json, std::string>
{
    // Copy path/data under lock, then do file I/O outside lock.
    bool is_file_backed = false;
    std::filesystem::path file_path;
    {
        std::shared_lock lock{m_mutex};
        const auto it = m_entries.find(id);
        if (it == m_entries.end()) {
            return std::unexpected{"ArtifactStore::retrieve: not found: " + type_safe::get(id)};
        }
        is_file_backed = it->second.info.is_file_backed;
        if (!is_file_backed) {
            return it->second.inline_data;
        }
        file_path = it->second.file_path;
    }

    try {
        std::ifstream in{file_path};
        if (!in) {
            return std::unexpected{"ArtifactStore::retrieve: cannot open " + file_path.string()};
        }
        return nlohmann::json::parse(in);
    }
    catch (const std::exception& e) {
        return std::unexpected{std::string{"ArtifactStore::retrieve: "} + e.what()};
    }
}

auto ArtifactStore::has(const ArtifactId& id) const -> bool
{
    std::shared_lock lock{m_mutex};
    return m_entries.contains(id);
}

auto ArtifactStore::list() const -> std::vector<ArtifactInfo>
{
    std::shared_lock lock{m_mutex};
    std::vector<ArtifactInfo> result;
    result.reserve(m_entries.size());
    for (const auto& [unused_key, entry] : m_entries) {
        result.push_back(entry.info);
    }
    return result;
}

void ArtifactStore::remove(const ArtifactId& id)
{
    std::unique_lock lock{m_mutex};
    m_entries.erase(id);
}

void ArtifactStore::clear()
{
    std::unique_lock lock{m_mutex};
    m_entries.clear();
}

}  // namespace attractor
