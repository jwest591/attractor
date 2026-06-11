#include <attractor/context.hpp>

#include <attractor/types.hpp>
#include <cmath>
#include <expected>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <type_safe/strong_typedef.hpp>

namespace attractor {

namespace {

// Recursively check for NaN or infinity at any nesting level.
bool has_non_finite_float(const nlohmann::json& j) noexcept
{
    if (j.is_number_float()) {
        const double d = j.get<double>();
        return std::isnan(d) || std::isinf(d);
    }
    if (j.is_array() || j.is_object()) {
        for (const auto& el : j) {
            if (has_non_finite_float(el)) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

auto Context::set(const ContextKey& key, nlohmann::json value) -> std::expected<void, std::string>
{
    if (has_non_finite_float(value)) {
        return std::unexpected{std::string{"Context::set: not JSON-serializable: non-finite float"}};
    }

    std::unique_lock lock{m_mutex};
    m_data[type_safe::get(key)] = std::move(value);
    return {};
}

auto Context::get(const ContextKey& key) const -> nlohmann::json
{
    std::shared_lock lock{m_mutex};
    const auto& k = type_safe::get(key);
    if (!m_data.contains(k)) {
        return nlohmann::json{};
    }
    return m_data[k];
}

auto Context::snapshot() const -> nlohmann::json
{
    std::shared_lock lock{m_mutex};
    return m_data;
}

void Context::merge_updates(const nlohmann::json& updates)
{
    if (!updates.is_object()) {
        return;
    }
    std::unique_lock lock{m_mutex};
    for (const auto& [k, v] : updates.items()) {
        if (!has_non_finite_float(v)) {
            m_data[k] = v;
        }
    }
}

}  // namespace attractor
