#include <attractor/dot_parser.hpp>
#include <attractor/engine.hpp>
#include <attractor/events.hpp>
#include <attractor/types.hpp>
#include <attractor/validator.hpp>
#include <attractor/backends/noop_backend.hpp>
#include "backends/claude_headless_backend.hpp"
#include "backends/claude_tmux_backend.hpp"
#include "backends/handoff_aware_backend.hpp"
#include "cli_utils.hpp"

#include <CLI/CLI.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <type_safe/strong_typedef.hpp>

using namespace attractor;

int main(int argc, char* argv[])
{
    CLI::App app{"attractor -- DOT-driven AI pipeline runner"};
    app.require_subcommand(1);

    auto* run = app.add_subcommand("run", "Execute a DOT pipeline file");

    std::string dot_file;
    run->add_option("file", dot_file, "Path to the .dot pipeline file")->required();

    std::string backend{"noop"};
    run->add_option("--backend", backend,
            "Backend for codergen nodes (noop, claude-headless, claude-tmux)")
        ->check(CLI::IsMember({"noop", "claude-headless", "claude-tmux"}));

    bool resume_run{false};
    run->add_flag("--resume", resume_run, "Resume from checkpoint.json in logs-root");

    std::string logs_root_str{".attractor"};
    run->add_option("--logs-root", logs_root_str, "Base directory for run artifacts");

    CLI11_PARSE(app, argc, argv);

    if (logs_root_str.empty()) {
        std::cerr << "error: --logs-root must not be empty\n";
        return 1;
    }

    std::string run_id_str;
    if (resume_run) {
        namespace fs = std::filesystem;
        std::error_code ec;
        std::string latest;
        for (const auto& entry : fs::directory_iterator{logs_root_str, ec}) {
            if (entry.is_directory()) {
                const auto name = entry.path().filename().string();
                if (name > latest) {
                    latest = name;
                }
            }
        }
        if (latest.empty()) {
            std::cerr << "error: no previous run found in " << logs_root_str << "\n";
            return 1;
        }
        run_id_str = std::move(latest);
    }
    else {
        run_id_str = generate_run_id();
    }

    logs_root_str = (std::filesystem::path{logs_root_str} / run_id_str).string();
    std::cout << "run-id: " << run_id_str << "\n"
              << "logs:   " << logs_root_str << "\n";

    // Read .dot file
    std::ifstream file_stream(dot_file);
    if (!file_stream) {
        std::cerr << "error: cannot open file: " << dot_file << "\n";
        return 1;
    }
    std::ostringstream oss;
    oss << file_stream.rdbuf();
    if (file_stream.bad()) {
        std::cerr << "error: I/O error while reading: " << dot_file << "\n";
        return 1;
    }
    const std::string source = oss.str();

    // Parse
    auto parse_result = parse_graph(source);
    if (!parse_result) {
        const auto& err = parse_result.error();
        std::cerr << "parse error";
        if (err.line > 0) {
            std::cerr << " at line " << err.line << ":" << err.column;
        }
        std::cerr << ": " << err.message << "\n";
        return 1;
    }
    const Graph graph = std::move(*parse_result);

    // Validate -- print all ERRORs to stderr; exit non-zero if any
    const auto diagnostics = validate(graph);
    bool has_errors         = false;
    for (const auto& d : diagnostics) {
        if (d.severity == Severity::error) {
            has_errors = true;
            std::cerr << "[" << type_safe::get(d.rule_id) << "]";
            if (d.node_id.has_value()) {
                std::cerr << " " << type_safe::get(*d.node_id);
                if (d.to_node_id.has_value()) {
                    std::cerr << " -> " << type_safe::get(*d.to_node_id);
                }
            }
            std::cerr << ": " << type_safe::get(d.message) << "\n";
        }
    }
    if (has_errors) {
        return 1;
    }

    // Write manifest before run (skip on resume to preserve original run_started timestamp)
    if (!resume_run && !write_manifest(logs_root_str, run_id_str, graph)) {
        std::cerr << "warning: failed to write manifest.json to " << logs_root_str << "\n";
    }

    // Build backend
    std::unique_ptr<CodergenBackend> backend_ptr;
    if (backend == "claude-headless") {
        backend_ptr = std::make_unique<HandoffAwareBackend>(
            std::make_unique<ClaudeCodeHeadlessBackend>(std::filesystem::path{logs_root_str}),
            std::filesystem::path{logs_root_str});
    }
    else if (backend == "claude-tmux") {
        backend_ptr = std::make_unique<HandoffAwareBackend>(
            std::make_unique<ClaudeCodeTmuxBackend>("tmux", std::filesystem::path{logs_root_str}),
            std::filesystem::path{logs_root_str});
    }
    else {
        backend_ptr = std::make_unique<NoOpBackend>();
    }

    Engine engine{std::move(backend_ptr), [](const Event& ev) { render_event(ev); }};
    const RunConfig config{.logs_root = LogsRoot{logs_root_str}, .resume = resume_run};
    const auto outcome = engine.run(graph, config);

    // Exit code mapping: 0 = success, 2 = partial_success, 1 = all other failures
    switch (outcome.status) {
        case StageStatus::success:
            return 0;
        case StageStatus::partial_success:
            // 2 = pipeline completed with partial results (some branches skipped/degraded)
            return 2;
        case StageStatus::fail:
        case StageStatus::retry:
        case StageStatus::skipped: {
            const auto reason = type_safe::get(outcome.failure_reason);
            if (!reason.empty()) {
                std::cerr << "pipeline failed: " << reason << "\n";
            }
            return 1;
        }
    }
    return 1;
}
