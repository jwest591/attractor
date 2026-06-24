#include "claude_tmux_backend.hpp"
#include "backend_utils.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <thread>

#include <cassert>
#include <cstdio>   // fopen, fclose, fgets
#include <cstdlib>  // system

namespace {

constexpr auto k_session_start_hard_deadline = std::chrono::seconds{10};
constexpr auto k_default_node_deadline = std::chrono::minutes{30};
constexpr auto k_poll_interval = std::chrono::milliseconds{200};

int tmux_system(const std::string& cmd)
{
    std::println(stderr, "{}", cmd);
    // NOLINT(cert-env33-c) -- subprocess via system() is required for tmux control
    return system(cmd.c_str());  // NOLINT(cert-env33-c)
}

std::string shell_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out += '\'';
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        }
        else {
            out += c;
        }
    }
    out += '\'';
    return out;
}

std::optional<std::string> extract_json_string(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\":";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    std::size_t start = pos + needle.size();
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t' || json[start] == '\n' || json[start] == '\r')) {
        ++start;
    }
    if (start >= json.size() || json[start] != '"') {
        return std::nullopt;
    }
    std::string out;
    for (std::size_t i = start + 1; i < json.size(); ++i) {
        if (json[i] == '"') {
            return out;
        }
        if (json[i] == '\\' && i + 1 < json.size()) {
            ++i;
            switch (json[i]) {
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            default:
                out += json[i];
                break;
            }
        }
        else {
            out += json[i];
        }
    }
    return std::nullopt;
}

}  // namespace

namespace attractor {

// RAII wrapper: creates a tmux window, kills it on scope exit
struct TmuxWindow {
    // tmux 3.0+ supports -e on new-window; injects ATTRACTOR_NODE_LOG_DIR and ATTRACTOR_CONTEXT_CRITICAL
    // shell_escape guards window name and log dir against paths with spaces or metacharacters
    TmuxWindow(std::string tmux_cmd, std::string session, std::string window, const std::string& node_log_dir,
               int context_critical_pct)
        : m_tmux_cmd{std::move(tmux_cmd)}
        , m_session{std::move(session)}
        , m_window{std::move(window)}
    {
        tmux_system(std::format(
            "{} new-window -t {} -n {} -e ATTRACTOR_NODE_LOG_DIR={} -e ATTRACTOR_CONTEXT_CRITICAL={}", m_tmux_cmd,
            m_session, shell_escape(m_window), shell_escape(node_log_dir), context_critical_pct));
    }

    ~TmuxWindow()
    {
        tmux_system(std::format("{} kill-window -t {}", m_tmux_cmd, shell_escape(m_session + ":" + m_window)));
    }

    TmuxWindow(const TmuxWindow&) = delete;
    TmuxWindow& operator=(const TmuxWindow&) = delete;
    TmuxWindow(TmuxWindow&&) = delete;
    TmuxWindow& operator=(TmuxWindow&&) = delete;

    std::string m_tmux_cmd;
    std::string m_session;
    std::string m_window;
};

ClaudeCodeTmuxBackend::ClaudeCodeTmuxBackend(std::string tmux_bin, std::filesystem::path logs_root)
    : m_tmux_bin{std::move(tmux_bin)}
    , m_session_id{logs_root.filename().string()}
    , m_logs_root{std::move(logs_root)}
    , m_scripts_dir{resolve_scripts_dir()}
{
    assert(!m_session_id.empty() && "logs_root must not be empty or end with a path separator");
    // Session name is derived from logs_root.filename() (the run_id)
    tmux_system(std::format("{} new-session -d -s {}", m_tmux_bin, shell_escape(m_session_id)));
}

ClaudeCodeTmuxBackend::~ClaudeCodeTmuxBackend()
{
    tmux_system(std::format("{} kill-session -t {}", m_tmux_bin, shell_escape(m_session_id)));
}

auto ClaudeCodeTmuxBackend::run(const Node& node, const PromptText& prompt, Context& ctx) const
    -> std::expected<LlmResponse, Outcome>
{
    // HandoffAwareBackend already called ctx.next_execution_counter(); read current value
    int counter = ctx.current_execution_counter();

    auto node_log_dir = derive_node_log_dir(m_logs_root, node.id, counter);

    {
        std::error_code ec;
        std::filesystem::create_directories(node_log_dir, ec);
        if (ec) {
            return std::unexpected(
                Outcome::fail(DiagnosticMessage{"tmux: cannot create node_log_dir: " + ec.message()}));
        }
    }

    auto now = std::chrono::steady_clock::now();
    auto deadline = node.timeout ? now + node.timeout->get_value() : now + k_default_node_deadline;

    const std::string window_name = type_safe::get(node.id) + "-" + std::to_string(counter);

    // RAII: destructor kills window on all exit paths
    TmuxWindow window{m_tmux_bin, m_session_id, window_name, node_log_dir.string(), m_context_critical_pct};

    const std::string settings_path = m_scripts_dir + "/att-tmux-backend.settings.json";
    const std::string claude_cmd = "claude " + shell_escape(type_safe::get(prompt)) + " --settings " +
                                   shell_escape(settings_path) + " --dangerously-skip-permissions";

    // Send text and Enter as separate calls; combining them can cause Enter to be swallowed
    const std::string send_text =
        std::format("{} send-keys -t {}:{} -l {}", m_tmux_bin, m_session_id, window_name, shell_escape(claude_cmd));
    const std::string send_enter = std::format("{} send-keys -t {}:{} Enter", m_tmux_bin, m_session_id, window_name);
    if (tmux_system(send_text) != 0 || tmux_system(send_enter) != 0) {
        return std::unexpected(Outcome::fail(DiagnosticMessage{"tmux: send-keys failed for " + window_name}));
    }

    // Poll for transcript.txt (written by SessionStart hook) to confirm the session started.
    // 10s hard cap or overall deadline, whichever is sooner.
    auto transcript_deadline = std::min(std::chrono::steady_clock::now() + k_session_start_hard_deadline, deadline);
    while (true) {
        if (std::chrono::steady_clock::now() >= transcript_deadline) {
            return std::unexpected(Outcome::fail(DiagnosticMessage{"tmux: SessionStart timeout for " + window_name}));
        }
        std::this_thread::sleep_for(k_poll_interval);
        std::error_code ec;
        if (std::filesystem::file_size(node_log_dir / "transcript.txt", ec) > 0) {
            break;
        }
    }

    // Poll for done.json written atomically by the Stop/StopFailure hook once all background
    // agents have completed. Format: {"status":"ok","message":"..."} or
    // {"status":"error","error_type":"...","message":"..."}.
    const auto done_file = node_log_dir / "done.json";
    while (true) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return std::unexpected(Outcome::fail(DiagnosticMessage{"timeout"}));
        }
        std::this_thread::sleep_for(k_poll_interval);

        FILE* fp = fopen(done_file.c_str(), "r");  // NOLINT(cppcoreguidelines-owning-memory)
        if (fp == nullptr) {
            continue;
        }
        char buf[65536];
        std::string content;
        while (fgets(buf, sizeof(buf), fp) != nullptr) {
            content += buf;
        }
        fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)

        auto status = extract_json_string(content, "status");
        if (!status) {
            continue;
        }
        if (*status == "ok") {
            return LlmResponse{extract_json_string(content, "message").value_or(std::string{})};
        }
        if (*status == "error") {
            auto etype = extract_json_string(content, "error_type").value_or(std::string{"unknown"});
            auto emsg  = extract_json_string(content, "message").value_or(std::string{});
            return std::unexpected(
                Outcome::fail(DiagnosticMessage{"tmux: API error [" + etype + "]: " + emsg}));
        }
    }
}

}  // namespace attractor
