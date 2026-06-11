#ifndef ATTRACTOR_ARTIFACT_STORE_HPP
#define ATTRACTOR_ARTIFACT_STORE_HPP

#include <attractor/types.hpp>
#include <expected>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <vector>

namespace attractor {

// Artifacts below this threshold are kept in memory; at or above, written to disk.
inline constexpr std::size_t k_artifact_file_threshold = 100 * 1024;  // 100 KB

struct ArtifactInfo {
    ArtifactId id;
    std::string name;
    std::size_t size_bytes{};
    bool is_file_backed{false};
};

// Thread-safe store for named pipeline artifacts.
// File-backed artifacts are written to {logs_root}/artifacts/{id}.json.
class ArtifactStore {
  public:
    // In-memory only (no file backing).
    ArtifactStore() = default;
    // With logs_root: large artifacts are file-backed.
    explicit ArtifactStore(LogsRoot logs_root);

    // Store artifact. Large artifacts (>= k_artifact_file_threshold) are written to disk
    // if logs_root was provided; otherwise stored in memory regardless of size.
    [[nodiscard]] auto store(const ArtifactId& id, std::string name, nlohmann::json data)
        -> std::expected<ArtifactInfo, std::string>;

    // Retrieve artifact by id. Reads from file if file-backed.
    [[nodiscard]] auto retrieve(const ArtifactId& id) const
        -> std::expected<nlohmann::json, std::string>;

    [[nodiscard]] auto has(const ArtifactId& id) const -> bool;
    [[nodiscard]] auto list() const -> std::vector<ArtifactInfo>;
    void remove(const ArtifactId& id);
    void clear();

  private:
    struct Entry {
        ArtifactInfo info;
        nlohmann::json inline_data;      // populated iff !info.is_file_backed
        std::filesystem::path file_path; // populated iff info.is_file_backed
    };

    LogsRoot m_logs_root;
    mutable std::shared_mutex m_mutex;
    std::map<ArtifactId, Entry> m_entries;
};

}  // namespace attractor

#endif  // ATTRACTOR_ARTIFACT_STORE_HPP
