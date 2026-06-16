#include <attractor/handlers/fan_in_handler.hpp>

#include <attractor/context.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <type_safe/strong_typedef.hpp>
#include <vector>

namespace attractor {

namespace {

int outcome_rank(StageStatus s) noexcept
{
    switch (s) {
    case StageStatus::success:
        return 0;
    case StageStatus::partial_success:
        return 1;
    case StageStatus::retry:
        return 2;
    case StageStatus::fail:
        return 3;
    case StageStatus::skipped:
        return 4;
    }
    return 5;
}

struct Candidate {
    std::string id;
    StageStatus status;
    double score;
    nlohmann::json entry;
};

}  // namespace

FanInHandler::FanInHandler(CodergenBackend* backend)
    : m_backend{backend}
{}

auto FanInHandler::execute(const Node& node, Context& ctx, const Graph& /*graph*/,
                           const LogsRoot& /*logs_root*/) const -> Outcome
{
    const nlohmann::json raw = ctx.get(ContextKey{"parallel.results"});
    if (raw.is_null() || !raw.is_array() || raw.empty()) {
        return Outcome::fail(DiagnosticMessage{"FanInHandler: parallel.results missing or empty"});
    }

    std::vector<Candidate> candidates;
    candidates.reserve(raw.size());
    for (const auto& entry : raw) {
        Candidate c;
        c.id    = entry.value("id", "");
        c.score = entry.value("score", 0.0);
        c.entry = entry;
        StageStatus st{};
        from_json(entry.at("status"), st);
        c.status = st;
        candidates.push_back(std::move(c));
    }

    std::optional<std::size_t> best_idx;
    bool from_llm = false;

    if (!node.prompt.empty() && m_backend != nullptr) {
        auto result = m_backend->run(node, node.prompt, ctx);
        if (result.has_value()) {
            const std::string llm_id = type_safe::get(*result);
            for (std::size_t i = 0; i < candidates.size(); ++i) {
                if (candidates[i].id == llm_id) {
                    best_idx = i;
                    from_llm = true;
                    break;
                }
            }
        }
    }

    if (!best_idx.has_value()) {
        std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            const int ra = outcome_rank(a.status);
            const int rb = outcome_rank(b.status);
            if (ra != rb) {
                return ra < rb;
            }
            if (a.score != b.score) {
                return a.score > b.score;
            }
            return a.id < b.id;
        });
        best_idx = 0;
    }

    const Candidate& best = candidates[*best_idx];

    if (best.status == StageStatus::fail) {
        if (from_llm) {
            return Outcome::fail(DiagnosticMessage{"FanInHandler: LLM-selected candidate has fail status"});
        }
        return Outcome::fail(DiagnosticMessage{"FanInHandler: all candidates failed"});
    }

    Outcome out{.context_updates = nlohmann::json::object()};
    out.context_updates["parallel.fan_in.best_id"]      = best.id;
    out.context_updates["parallel.fan_in.best_outcome"] = best.entry;
    out.notes = HandlerNote{"FanInHandler: selected best candidate: " + best.id};
    return out;
}

}  // namespace attractor
