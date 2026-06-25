#ifndef ATTRACTOR_CONTEXT_HPP
#define ATTRACTOR_CONTEXT_HPP

#include <attractor/types.hpp>
#include <atomic>
#include <expected>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>

namespace attractor {

class Context {
  public:
    [[nodiscard]] auto set(const ContextKey& key, nlohmann::json value)
        -> std::expected<void, std::string>;

    [[nodiscard]] auto get(const ContextKey& key) const -> nlohmann::json;

    [[nodiscard]] auto snapshot() const -> nlohmann::json;

    void merge_updates(const nlohmann::json& updates);

    [[nodiscard]] int next_execution_counter()
    {
        return m_execution_counter.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    [[nodiscard]] int current_execution_counter() const
    {
        return m_execution_counter.load(std::memory_order_relaxed);
    }

    void set_execution_counter(int v)
    {
        m_execution_counter.store(v, std::memory_order_relaxed);
    }

  private:
    mutable std::shared_mutex m_mutex;
    nlohmann::json m_data = nlohmann::json::object();
    std::atomic<int> m_execution_counter{0};
};

}  // namespace attractor

#endif  // ATTRACTOR_CONTEXT_HPP
