#include <attractor/handlers/fan_in_handler.hpp>

#include <attractor/context.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>

#include <algorithm>
#include <cmath>
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
    default:
        return 5;
    }
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
                           const RunConfig& /*run_config*/) const -> Outcome
{
    const nlohmann::json raw = ctx.get(ContextKey{"parallel.results"});
    if (raw.is_null() || !raw.is_array() || raw.empty()) {
        return Outcome::fail(DiagnosticMessage{"FanInHandler: parallel.results missing or empty"});
    }

    std::vector<Candidate> candidates;
    candidates.reserve(raw.size());
    std::string score_diagnostics;

    for (const auto& entry : raw) {
        Candidate c;
        c.id    = entry.value("id", "");
        c.entry = entry;
        StageStatus st{};
        if (!entry.contains("status")) {
            score_diagnostics += "branch '" + c.id + "' status missing; ";
            st = StageStatus::fail;
        } else {
            from_json(entry.at("status"), st);
        }
        c.status = st;

        if (!entry.contains("score") || !entry.at("score").is_number()) {
            score_diagnostics += "branch '" + c.id + "' score missing or non-numeric; ";
            c.score = 0.0;
        } else {
            c.score = entry.at("score").get<double>();
        }
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
            if (std::abs(a.score - b.score) >= 1e-9) {
                return a.score > b.score;
            }
            return a.id < b.id;
        });
        best_idx = 0;
    }

    const Candidate& best = candidates[*best_idx];

    if (best.status == StageStatus::fail) {
        if (from_llm) {
            Outcome out = Outcome::fail(DiagnosticMessage{"FanInHandler: LLM-selected candidate has fail status"});
            if (!score_diagnostics.empty())
                out.notes = HandlerNote{score_diagnostics};
            return out;
        }
        Outcome out = Outcome::fail(DiagnosticMessage{"FanInHandler: all candidates failed"});
        if (!score_diagnostics.empty())
            out.notes = HandlerNote{score_diagnostics};
        return out;
    }

    Outcome out{.context_updates = nlohmann::json::object()};
    out.context_updates["parallel.fan_in.best_id"]      = best.id;
    out.context_updates["parallel.fan_in.best_outcome"] = best.entry;

    std::string notes_str = "FanInHandler: selected best candidate: " + best.id;
    if (!score_diagnostics.empty()) {
        notes_str += "; " + score_diagnostics;
    }
    out.notes = HandlerNote{notes_str};
    return out;
}

}  // namespace attractor
