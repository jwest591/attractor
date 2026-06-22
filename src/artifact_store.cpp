#include <attractor/artifact_store.hpp>

#include <attractor/types.hpp>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    {
        const std::string& raw_id = type_safe::get(id);
        if (raw_id.empty()) {
            return std::unexpected{"ArtifactStore::store: ArtifactId must not be empty"};
        }
        if (raw_id.find('/') != std::string::npos || raw_id.find('\\') != std::string::npos
            || raw_id.find("..") != std::string::npos) {
            return std::unexpected{
                "ArtifactStore::store: ArtifactId contains invalid characters: " + raw_id};
        }
    }

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
                    std::error_code ec;
                    std::filesystem::remove(file, ec);
                    return std::unexpected{"ArtifactStore::store: cannot open " + file.string()};
                }
                out << serialized;
                if (!out.flush()) {
                    std::error_code ec;
                    std::filesystem::remove(file, ec);
                    return std::unexpected{"ArtifactStore::store: write failed for " + file.string()};
                }
            }
            entry.file_path = file;
        }
        else {
            entry.inline_data = std::move(data);
        }

        std::filesystem::path old_file_to_delete;
        {
            std::unique_lock lock{m_mutex};
            auto it = m_entries.find(id);
            if (it != m_entries.end() && it->second.info.is_file_backed && !entry.info.is_file_backed) {
                old_file_to_delete = it->second.file_path;
            }
            m_entries[id] = std::move(entry);
        }
        if (!old_file_to_delete.empty()) {
            std::error_code ec;
            std::filesystem::remove(old_file_to_delete, ec);
            if (ec) {
                std::cerr << "ArtifactStore::store: failed to delete old backing file "
                          << old_file_to_delete << ": " << ec.message() << '\n';
            }
        }
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
    std::filesystem::path file_to_delete;
    {
        std::unique_lock lock{m_mutex};
        auto it = m_entries.find(id);
        if (it != m_entries.end()) {
            if (it->second.info.is_file_backed) {
                file_to_delete = it->second.file_path;
            }
            m_entries.erase(it);
        }
    }
    if (!file_to_delete.empty()) {
        std::error_code ec;
        std::filesystem::remove(file_to_delete, ec);
        if (ec) {
            std::cerr << "ArtifactStore::remove: failed to delete " << file_to_delete
                      << ": " << ec.message() << '\n';
        }
    }
}

void ArtifactStore::clear()
{
    std::vector<std::filesystem::path> files_to_delete;
    {
        std::unique_lock lock{m_mutex};
        for (const auto& [unused_key, entry] : m_entries) {
            if (entry.info.is_file_backed) {
                files_to_delete.push_back(entry.file_path);
            }
        }
        m_entries.clear();
    }
    for (const auto& file : files_to_delete) {
        std::error_code ec;
        std::filesystem::remove(file, ec);
        if (ec) {
            std::cerr << "ArtifactStore::clear: failed to delete " << file
                      << ": " << ec.message() << '\n';
        }
    }
}

}  // namespace attractor
