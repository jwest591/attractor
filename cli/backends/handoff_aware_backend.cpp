#include "handoff_aware_backend.hpp"
#include "backend_utils.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace attractor {

HandoffAwareBackend::HandoffAwareBackend(std::shared_ptr<CodergenBackend> inner,
                                         int max_handoffs)
    : m_inner{std::move(inner)}, m_max_handoffs{max_handoffs}
{
    assert(max_handoffs >= 0);
}

auto HandoffAwareBackend::run(const Node& node, const PromptText& prompt,
                               Context& ctx) const
    -> std::expected<LlmResponse, Outcome>
{
    auto path_result = compute_handoff_path(derive_session_name(node));
    if (!path_result) {
        return std::unexpected(Outcome::fail(DiagnosticMessage{
            "handoff: cannot create .attractor directory: " + path_result.error()}));
    }
    const std::string& handoff_path = *path_result;

    // Clear any stale handoff file from a prior incomplete run before we start
    { std::error_code ec; std::filesystem::remove(std::filesystem::path(handoff_path), ec); }

    PromptText current_prompt = prompt;

    for (int attempt = 0; attempt <= m_max_handoffs; ++attempt) {
        auto result = m_inner->run(node, current_prompt, ctx);

        if (!result) {
            // Inner error: clean up any handoff file the inner backend may have written
            std::error_code ec;
            std::filesystem::remove(std::filesystem::path(handoff_path), ec);
            return result;
        }

        if (!std::filesystem::exists(handoff_path)) return result;  // clean completion

        // Handoff file present -- context ceiling was reached
        if (attempt == m_max_handoffs) {
            return std::unexpected(Outcome::fail(DiagnosticMessage{
                "handoff: max handoffs (" + std::to_string(m_max_handoffs)
                + ") exhausted; handoff file: " + handoff_path}));
        }

        std::ifstream f(handoff_path);
        if (!f.is_open()) {
            return std::unexpected(Outcome::fail(
                DiagnosticMessage{"handoff: handoff file disappeared: " + handoff_path}));
        }
        std::string content{std::istreambuf_iterator<char>(f), {}};
        const auto first_nonws = content.find_first_not_of(" \t\r\n");
        if (first_nonws == std::string::npos) {
            std::error_code ec;
            std::filesystem::remove(std::filesystem::path(handoff_path), ec);
            return std::unexpected(Outcome::fail(
                DiagnosticMessage{"handoff: handoff file is empty: " + handoff_path}));
        }
        content.erase(0, first_nonws);
        content.erase(content.find_last_not_of(" \t\r\n") + 1);

        // Leave the handoff file on disk -- ClaudeCodeTmuxBackend checks for it at the
        // start of the next run() call to know it must /clear before re-prompting.
        // For headless, each run() spawns a fresh process so no explicit clear is needed.
        current_prompt = PromptText{content};
    }

    // Unreachable: loop exits via return inside
    return std::unexpected(Outcome::fail(DiagnosticMessage{"handoff: internal loop error"}));
}

} // namespace attractor
