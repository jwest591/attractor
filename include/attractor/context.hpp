#ifndef ATTRACTOR_CONTEXT_HPP
#define ATTRACTOR_CONTEXT_HPP

#include <attractor/types.hpp>
#include <expected>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>

namespace attractor {

class Context {
  public:
    // Set a key-value pair; validates JSON-serializability at set-time (NFR2).
    // Returns std::unexpected<std::string> if value.dump() would throw.
    [[nodiscard]] auto set(const ContextKey& key, nlohmann::json value)
        -> std::expected<void, std::string>;

    // Get value by key; returns JSON null if key is absent.
    [[nodiscard]] auto get(const ContextKey& key) const -> nlohmann::json;

    // Full KV state as a JSON object (for edge condition evaluation, checkpoint).
    [[nodiscard]] auto snapshot() const -> nlohmann::json;

    // Merge all entries from a JSON object; silently skips non-serializable values.
    void merge_updates(const nlohmann::json& updates);

  private:
    mutable std::shared_mutex m_mutex;
    nlohmann::json m_data = nlohmann::json::object();
};

}  // namespace attractor

#endif  // ATTRACTOR_CONTEXT_HPP
