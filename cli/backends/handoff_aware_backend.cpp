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

HandoffAwareBackend::HandoffAwareBackend(std::unique_ptr<CodergenBackend> inner,
                                         std::filesystem::path logs_root,
                                         int max_handoffs)
    : m_inner{std::move(inner)}, m_logs_root{std::move(logs_root)}, m_max_handoffs{max_handoffs}
{
    assert(max_handoffs >= 0);
}

auto HandoffAwareBackend::run(const Node& node, const PromptText& prompt,
                               Context& ctx) const
    -> std::expected<LlmResponse, Outcome>
{
    PromptText current_prompt = prompt;

    for (int attempt = 0; attempt <= m_max_handoffs; ++attempt) {
        int counter = ctx.next_execution_counter();
        auto node_log_dir = derive_node_log_dir(m_logs_root, node.id, counter);

        auto result = m_inner->run(node, current_prompt, ctx);

        if (!result) return result;

        const auto handoff_file = node_log_dir / "handoff.md";
        std::error_code exists_ec;
        const bool handoff_exists = std::filesystem::exists(handoff_file, exists_ec);
        if (exists_ec) {
            return std::unexpected(Outcome::fail(
                DiagnosticMessage{"handoff: cannot check handoff file: " + exists_ec.message()}));
        }
        if (!handoff_exists) return result;  // clean completion

        if (attempt == m_max_handoffs) {
            return std::unexpected(Outcome::fail(DiagnosticMessage{
                "handoff: max handoffs (" + std::to_string(m_max_handoffs) + ") exhausted"}));
        }

        std::ifstream f(handoff_file);
        if (!f.is_open()) {
            return std::unexpected(Outcome::fail(
                DiagnosticMessage{"handoff: handoff file disappeared: " + handoff_file.string()}));
        }
        std::string content{std::istreambuf_iterator<char>(f), {}};
        const auto first_nonws = content.find_first_not_of(" \t\r\n");
        if (first_nonws == std::string::npos) {
            std::error_code ec;
            std::filesystem::remove(handoff_file, ec);
            return std::unexpected(Outcome::fail(
                DiagnosticMessage{"handoff: handoff file is empty: " + handoff_file.string()}));
        }
        content.erase(0, first_nonws);
        content.erase(content.find_last_not_of(" \t\r\n") + 1);

        std::error_code ec;
        std::filesystem::remove(handoff_file, ec);

        current_prompt = PromptText{content};
    }

    return std::unexpected(Outcome::fail(DiagnosticMessage{"handoff: internal loop error"}));
}

} // namespace attractor
