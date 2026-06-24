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
#include <fstream>
#include <optional>
#include <print>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <cassert>
#include <cstdint>
#include <cstdio>   // fopen, fclose, fgets
#include <cstdlib>  // system
#include <limits>

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

[[nodiscard]] auto read_jsonl_increment(const std::string& path, std::size_t offset)
    -> std::pair<std::vector<std::string>, std::size_t>
{
    // RAII wrapper: guarantees fclose even if push_back/string ops throw std::bad_alloc
    auto fp_deleter = [](FILE* f) noexcept { if (f != nullptr) fclose(f); };  // NOLINT(cppcoreguidelines-owning-memory)
    std::unique_ptr<FILE, decltype(fp_deleter)> fp{                            // NOLINT(cppcoreguidelines-owning-memory)
        fopen(path.c_str(), "r"), fp_deleter};
    if (!fp) {
        return {{}, offset};
    }
    if (fseek(fp.get(), static_cast<long>(offset), SEEK_SET) != 0) {
        return {{}, offset};
    }
    std::vector<std::string> lines;
    char buf[65536];
    std::string partial;
    while (fgets(buf, sizeof(buf), fp.get()) != nullptr) {
        partial += buf;
        std::size_t pos;
        while ((pos = partial.find('\n')) != std::string::npos) {
            lines.push_back(partial.substr(0, pos));
            partial.erase(0, pos + 1);
        }
    }
    // Flush any trailing content not terminated by '\n' (some JSONL writers omit the final newline)
    if (!partial.empty()) {
        lines.push_back(std::move(partial));
    }
    const auto raw_offset = ftell(fp.get());
    if (raw_offset < 0) {
        return {{}, offset};
    }
    return {std::move(lines), static_cast<std::size_t>(raw_offset)};
}

[[nodiscard]] auto extract_usage_input_tokens(const std::string& line) -> std::optional<uint64_t>
{
    if (line.find("\"type\":\"usage\"") == std::string::npos) {
        return std::nullopt;
    }
    const std::string needle = "\"input_tokens\":";
    const auto pos = line.find(needle);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    std::size_t i = pos + needle.size();
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        ++i;
    }
    if (i >= line.size() || line[i] < '0' || line[i] > '9') {
        return std::nullopt;
    }
    uint64_t val = 0;
    while (i < line.size() && line[i] >= '0' && line[i] <= '9') {
        const uint64_t digit = static_cast<uint64_t>(line[i++] - '0');
        if (val > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            return std::numeric_limits<uint64_t>::max();
        }
        val = val * 10 + digit;
    }
    return val;
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

ClaudeCodeTmuxBackend::ClaudeCodeTmuxBackend(std::string tmux_bin, std::filesystem::path logs_root,
                                             uint64_t ceiling_tokens, int max_ceiling_handoffs)
    : m_tmux_bin{std::move(tmux_bin)}
    , m_session_id{logs_root.filename().string()}
    , m_logs_root{std::move(logs_root)}
    , m_scripts_dir{resolve_scripts_dir()}
    , m_ceiling_tokens{ceiling_tokens}
    , m_max_ceiling_handoffs{max_ceiling_handoffs}
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

    const auto transcript_txt = node_log_dir / "transcript.txt";

    // Poll for transcript.txt (written by SessionStart hook) to confirm the session started.
    // 10s hard cap or overall deadline, whichever is sooner.
    auto poll_transcript = [&]() -> std::expected<std::string, Outcome> {
        auto transcript_deadline =
            std::min(std::chrono::steady_clock::now() + k_session_start_hard_deadline, deadline);
        while (true) {
            if (std::chrono::steady_clock::now() >= transcript_deadline) {
                return std::unexpected(
                    Outcome::fail(DiagnosticMessage{"tmux: SessionStart timeout for " + window_name}));
            }
            std::this_thread::sleep_for(k_poll_interval);
            std::error_code ec;
            if (std::filesystem::file_size(transcript_txt, ec) > 0) {
                break;
            }
        }
        std::string jsonl_path;
        {
            std::ifstream f{transcript_txt};
            std::getline(f, jsonl_path);
            if (!jsonl_path.empty() && jsonl_path.back() == '\r') {
                jsonl_path.pop_back();
            }
        }
        return jsonl_path;
    };

    auto jsonl_result = poll_transcript();
    if (!jsonl_result) {
        return std::unexpected(jsonl_result.error());
    }
    std::string jsonl_path = std::move(*jsonl_result);

    // Poll for done.json written atomically by the Stop/StopFailure hook once all background
    // agents have completed. Format: {"status":"ok","message":"..."} or
    // {"status":"error","error_type":"...","message":"..."}.
    const auto done_file = node_log_dir / "done.json";
    uint64_t accumulated_tokens = 0;
    std::size_t jsonl_offset = 0;
    int handoff_count = 0;

    while (true) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return std::unexpected(Outcome::fail(DiagnosticMessage{"timeout"}));
        }
        std::this_thread::sleep_for(k_poll_interval);

        // Read new JSONL lines and accumulate max usage tokens.
        if (!jsonl_path.empty()) {
            auto [new_lines, new_offset] = read_jsonl_increment(jsonl_path, jsonl_offset);
            jsonl_offset = new_offset;
            for (const auto& line : new_lines) {
                if (auto tokens = extract_usage_input_tokens(line)) {
                    accumulated_tokens = std::max(accumulated_tokens, *tokens);
                }
            }
        }

        // Ceiling detection: trigger handoff if threshold exceeded.
        if (m_ceiling_tokens > 0 && accumulated_tokens >= m_ceiling_tokens) {
            if (handoff_count >= m_max_ceiling_handoffs) {
                return std::unexpected(Outcome::fail(DiagnosticMessage{
                    "tmux: ceiling handoff limit (" + std::to_string(m_max_ceiling_handoffs) +
                    ") exhausted"}));
            }

            ++handoff_count;

            // Delete transcript.txt and ctx-usage.json so the new SessionStart hook can write fresh ones.
            // transcript.txt MUST be deleted or att-session-start.sh will fail when /clear fires.
            std::error_code ec;
            std::filesystem::remove(transcript_txt, ec);
            if (ec) {
                return std::unexpected(Outcome::fail(DiagnosticMessage{
                    "tmux: handoff failed: cannot remove transcript.txt: " + ec.message()}));
            }
            std::filesystem::remove(node_log_dir / "ctx-usage.json", ec);

            // Send /clear to the current tmux window (literal, not shell-escaped).
            const std::string clear_text =
                std::format("{} send-keys -t {}:{} -l /clear", m_tmux_bin, m_session_id, window_name);
            const std::string clear_enter =
                std::format("{} send-keys -t {}:{} Enter", m_tmux_bin, m_session_id, window_name);
            if (tmux_system(clear_text) != 0 || tmux_system(clear_enter) != 0) {
                return std::unexpected(Outcome::fail(DiagnosticMessage{
                    "tmux: handoff failed: /clear send-keys failed for " + window_name}));
            }

            // Wait for new transcript.txt from the fresh SessionStart hook.
            auto new_jsonl_result = poll_transcript();
            if (!new_jsonl_result) {
                return std::unexpected(new_jsonl_result.error());
            }
            jsonl_path = std::move(*new_jsonl_result);
            if (jsonl_path.empty()) {
                return std::unexpected(Outcome::fail(DiagnosticMessage{
                    "tmux: handoff failed: new transcript.txt contained no JSONL path"}));
            }

            // Inject context summary into the new session.
            const std::string summary =
                "Continuing pipeline node " + type_safe::get(node.id) +
                ". Context window was cleared (token ceiling reached). "
                "Continue your work from where the previous session left off.";
            const std::string summary_text = std::format("{} send-keys -t {}:{} -l {}", m_tmux_bin, m_session_id,
                                                         window_name, shell_escape(summary));
            const std::string summary_enter =
                std::format("{} send-keys -t {}:{} Enter", m_tmux_bin, m_session_id, window_name);
            if (tmux_system(summary_text) != 0 || tmux_system(summary_enter) != 0) {
                return std::unexpected(Outcome::fail(DiagnosticMessage{
                    "tmux: handoff failed: summary send-keys failed for " + window_name}));
            }

            accumulated_tokens = 0;
            jsonl_offset = 0;
            continue;
        }

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
