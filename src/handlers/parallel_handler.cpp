#include <attractor/handlers/parallel_handler.hpp>

#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>

#include <exec/async_scope.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/unless_stop_requested.hpp>
#include <stdexec/execution.hpp>

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <type_safe/strong_typedef.hpp>
#include <vector>

namespace ex = stdexec;

namespace attractor {

ParallelHandler::ParallelHandler(RunFn run_fn) : m_run_fn{std::move(run_fn)}
{
    assert(static_cast<bool>(m_run_fn));
}

auto ParallelHandler::execute(const Node& node, Context& /*ctx*/,
                              const Graph& graph, const RunConfig& run_config) const -> Outcome
{
    std::vector<NodeId> branch_starts;
    for (const auto& edge : graph.edges) {
        if (edge.from == node.id)
            branch_starts.push_back(edge.to);
    }

    if (branch_starts.empty())
        return Outcome::fail(DiagnosticMessage{"ParallelHandler: no outgoing branches"});

    const auto max_par = static_cast<std::uint32_t>(node.max_parallel.get_value());

    RunConfig branch_config{run_config};
    branch_config.resume = false;

    if (node.join_policy == JoinPolicy::first_success) {
        std::vector<Outcome> results(branch_starts.size(), Outcome{.status = StageStatus::skipped});
        std::atomic<bool> first_success_found{false};

        bool fan_out_completed = false;
        {
            exec::static_thread_pool pool(max_par);
            auto sched = pool.get_scheduler();
            exec::async_scope scope;

            for (std::size_t i = 0; i < branch_starts.size(); ++i) {
                scope.spawn(
                    ex::starts_on(sched,
                        ex::just(i, branch_starts[i])
                            | exec::unless_stop_requested()
                            | ex::then([this, &graph, &branch_config, &results, &scope, &first_success_found]
                                       (std::size_t idx, NodeId start) {
                                  results[idx] = m_run_fn(graph, start, branch_config);
                                  if (results[idx].status == StageStatus::success) {
                                      first_success_found = true;
                                      scope.request_stop();
                                  }
                              })));
            }

            const auto sync_result = ex::sync_wait(scope.on_empty());
            fan_out_completed = sync_result.has_value();
        }

        if (!fan_out_completed)
            return Outcome::fail(DiagnosticMessage{"ParallelHandler: fan-out stopped unexpectedly"});

        nlohmann::json results_json = nlohmann::json::array();
        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            if (r.status == StageStatus::skipped)
                continue;
            nlohmann::json entry = nlohmann::json::object();
            entry["id"] = type_safe::get(branch_starts[i]);
            nlohmann::json status_j;
            to_json(status_j, r.status);
            entry["status"] = status_j;
            entry["failure_reason"] = type_safe::get(r.failure_reason);
            entry["notes"] = type_safe::get(r.notes);
            if (r.context_updates.is_object() && r.context_updates.contains("parallel.score")
                    && r.context_updates.at("parallel.score").is_number()) {
                const double score = r.context_updates.at("parallel.score").get<double>();
                if (!std::isfinite(score)) {
                    if (!first_success_found) {
                        return Outcome::fail(DiagnosticMessage{
                            "ParallelHandler: branch " + type_safe::get(branch_starts[i])
                                + " produced non-finite score (NaN or infinity)"});
                    }
                    entry["score"] = 0.0;
                } else {
                    entry["score"] = score;
                }
            } else {
                entry["score"] = 0.0;
            }
            results_json.push_back(entry);
        }

        Outcome out;
        out.context_updates["parallel.results"] = results_json;
        if (first_success_found) {
            out.status = StageStatus::success;
            out.notes = HandlerNote{"ParallelHandler: first_success found"};
        } else {
            out.status = StageStatus::fail;
            out.failure_reason = DiagnosticMessage{"ParallelHandler: all branches failed (first_success)"};
        }
        return out;
    }

    // wait_all policy
    std::vector<Outcome> results(branch_starts.size());
    bool fan_out_completed = false;
    {
        exec::static_thread_pool pool(max_par);
        auto sched = pool.get_scheduler();
        exec::async_scope scope;

        for (std::size_t i = 0; i < branch_starts.size(); ++i) {
            scope.spawn(
                ex::starts_on(sched,
                    ex::just(i, branch_starts[i])
                        | ex::then([this, &graph, &branch_config, &results]
                                   (std::size_t idx, NodeId start) {
                              results[idx] = m_run_fn(graph, start, branch_config);
                          })));
        }

        const auto sync_result = ex::sync_wait(scope.on_empty());
        fan_out_completed = sync_result.has_value();
    }

    if (!fan_out_completed)
        return Outcome::fail(DiagnosticMessage{"ParallelHandler: fan-out stopped unexpectedly"});

    nlohmann::json results_json = nlohmann::json::array();
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        nlohmann::json entry = nlohmann::json::object();
        entry["id"] = type_safe::get(branch_starts[i]);
        nlohmann::json status_j;
        to_json(status_j, r.status);
        entry["status"] = status_j;
        entry["failure_reason"] = type_safe::get(r.failure_reason);
        entry["notes"] = type_safe::get(r.notes);
        if (r.context_updates.is_object() && r.context_updates.contains("parallel.score")
                && r.context_updates.at("parallel.score").is_number()) {
            const double score = r.context_updates.at("parallel.score").get<double>();
            if (!std::isfinite(score)) {
                return Outcome::fail(DiagnosticMessage{
                    "ParallelHandler: branch " + type_safe::get(branch_starts[i])
                        + " produced non-finite score (NaN or infinity)"});
            }
            entry["score"] = score;
        } else {
            entry["score"] = 0.0;
        }
        results_json.push_back(entry);
    }

    std::size_t fail_count = 0;
    std::size_t partial_count = 0;
    for (const auto& r : results) {
        if (r.status == StageStatus::fail)
            ++fail_count;
        else if (r.status == StageStatus::partial_success)
            ++partial_count;
    }

    Outcome out;
    out.context_updates["parallel.results"] = results_json;

    if (fail_count == 0 && partial_count == 0) {
        out.status = StageStatus::success;
        out.notes = HandlerNote{"ParallelHandler: all branches succeeded"};
    } else if (fail_count == 0) {
        out.status = StageStatus::partial_success;
        out.notes = HandlerNote{"ParallelHandler: all branches partial"};
    } else {
        out.status = StageStatus::partial_success;
        out.notes = HandlerNote{"ParallelHandler: " + std::to_string(fail_count) + " branch(es) failed"};
    }
    return out;
}

}  // namespace attractor
