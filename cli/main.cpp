#include <attractor/dot_parser.hpp>
#include <attractor/engine.hpp>
#include <attractor/events.hpp>
#include <attractor/types.hpp>
#include <attractor/validator.hpp>
#include <attractor/backends/noop_backend.hpp>
#include "backends/claude_headless_backend.hpp"
#include "backends/claude_tmux_backend.hpp"

#include <unistd.h>

#include <CLI/CLI.hpp>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <type_safe/strong_typedef.hpp>
#include <variant>

using namespace attractor;

namespace {

void render_event(const Event& event)
{
    std::visit(
        [](auto&& ev) {
            using T = std::decay_t<decltype(ev)>;
            if constexpr (std::is_same_v<T, StageStarted>) {
                std::cout << "[stage " << (ev.index + 1) << "] started: "
                          << type_safe::get(ev.id) << "\n";
            }
            else if constexpr (std::is_same_v<T, StageCompleted>) {
                std::cout << "[stage " << (ev.index + 1) << "] completed: "
                          << type_safe::get(ev.id) << "\n";
            }
            else {
                static_assert(std::is_same_v<T, void>,
                    "render_event: unhandled Event variant -- update this visitor");
            }
        },
        event);
}

[[nodiscard]] std::string generate_run_id()
{
    const auto now = std::chrono::system_clock::now();
    const auto tt  = std::chrono::system_clock::to_time_t(now);
    char buf[32]{};
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    auto* const tm_ptr = std::gmtime(&tt);
    if (tm_ptr != nullptr) {
        std::strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", tm_ptr);
    }
    return std::string{buf} + "-" + std::to_string(static_cast<unsigned>(::getpid()));
}

bool write_manifest(const std::string& logs_root_str, const std::string& run_id, const Graph& graph)
{
    namespace fs = std::filesystem;
    const fs::path dir{logs_root_str};
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        return false;
    }
    std::ofstream f{dir / "manifest.json"};
    if (!f) {
        return false;
    }
    nlohmann::json j;
    j["run_id"]     = run_id;
    j["graph_id"]   = type_safe::get(graph.digraph_id);
    j["node_count"] = static_cast<int>(graph.nodes.size());
    j["edge_count"] = static_cast<int>(graph.edges.size());
    const auto now  = std::chrono::system_clock::now();
    const auto tt   = std::chrono::system_clock::to_time_t(now);
    char buf[32]{};
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    auto* const tm_ptr = std::gmtime(&tt);
    if (tm_ptr != nullptr) {
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_ptr);
    }
    j["run_started"] = buf;
    f << j.dump(2) << "\n";
    return true;
}

}  // namespace

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

    std::string logs_root_str{"./logs"};
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
    std::ostringstream buf;
    buf << file_stream.rdbuf();
    const std::string source = buf.str();

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
    std::shared_ptr<CodergenBackend> backend_ptr;
    if (backend == "claude-headless") {
        backend_ptr = std::make_shared<ClaudeCodeHeadlessBackend>();
    }
    else if (backend == "claude-tmux") {
        backend_ptr = std::make_shared<ClaudeCodeTmuxBackend>();
    }
    else {
        backend_ptr = std::make_shared<NoOpBackend>();
    }

    Engine engine{std::move(backend_ptr), [](const Event& ev) { render_event(ev); }};
    const RunConfig config{.logs_root = LogsRoot{logs_root_str}, .resume = resume_run};
    const auto outcome = engine.run(graph, config);

    if (outcome.status != StageStatus::success) {
        const auto reason = type_safe::get(outcome.failure_reason);
        if (!reason.empty()) {
            std::cerr << "pipeline failed: " << reason << "\n";
        }
        return 1;
    }
    return 0;
}
