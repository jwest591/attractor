#include "claude_tmux_backend.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

#include <cctype>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <cstdio>   // fopen, fclose, fgets, fseek, ftell
#include <cstdlib>  // system

namespace {

// Fallback timeouts for nodes that set no explicit node.timeout.
constexpr auto k_session_start_timeout = std::chrono::seconds{30};
constexpr auto k_response_timeout      = std::chrono::minutes{5};

int tmux_system(const std::string& cmd) { return system(cmd.c_str()); }  // NOLINT(cert-env33-c) -- subprocess via system() is required for tmux control

bool has_session(const std::string& tmux, const std::string& name)
{
    return tmux_system(tmux + " has-session -t " + name + " 2>/dev/null") == 0;
}

std::string shell_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out += '\'';
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else           out += c;
    }
    out += '\'';
    return out;
}

std::string derive_session_name(const attractor::Node& node)
{
    std::string raw = node.thread_id.has_value()
        ? type_safe::get(*node.thread_id)
        : type_safe::get(node.id);
    std::string name = "att-" + raw;
    for (char& c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '-';
    }
    return name;
}

std::string poll_transcript_path(
    const std::string& name,
    std::optional<std::chrono::steady_clock::time_point> deadline)
{
    constexpr auto poll_interval = std::chrono::milliseconds{200};
    const std::string marker = "/tmp/att-" + name + "-transcript.txt";
    const auto eff_deadline = deadline.value_or(
        std::chrono::steady_clock::now() + k_session_start_timeout);

    while (true) {
        if (std::chrono::steady_clock::now() >= eff_deadline) return {};
        std::this_thread::sleep_for(poll_interval);

        FILE* fp = fopen(marker.c_str(), "r");  // NOLINT(cppcoreguidelines-owning-memory) -- FILE* closed in this scope
        if (fp == nullptr) continue;
        char buf[4096] = {};
        fgets(buf, sizeof(buf), fp);
        fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory) -- matches fopen above

        std::string path(buf);
        if (!path.empty() && path.back() == '\n') path.pop_back();
        if (!path.empty()) return path;
    }
}

std::string create_session(
    const std::string& tmux,
    const std::string& name,
    const std::string& settings_path,
    std::optional<std::chrono::steady_clock::time_point> deadline)
{
    const std::string cmd = tmux + " new-session -d -s " + name
        + " 'claude -n " + name
        + " --settings " + shell_escape(settings_path)
        + " --dangerously-skip-permissions' 2>/dev/null";
    if (tmux_system(cmd) != 0) return {};
    return poll_transcript_path(name, deadline);
}

long record_offset(const std::string& transcript_path)
{
    FILE* fp = fopen(transcript_path.c_str(), "r");  // NOLINT(cppcoreguidelines-owning-memory) -- FILE* closed in this scope
    if (fp == nullptr) return 0L;
    fseek(fp, 0L, SEEK_END);
    long off = ftell(fp);
    fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory) -- matches fopen above
    return off < 0 ? 0L : off;
}

std::optional<std::string> extract_json_string(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\":\"";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) return std::nullopt;
    std::string out;
    for (std::size_t i = pos + needle.size(); i < json.size(); ++i) {
        if (json[i] == '"') return out;
        if (json[i] == '\\' && i + 1 < json.size()) {
            ++i;
            switch (json[i]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += json[i]; break;
            }
        } else {
            out += json[i];
        }
    }
    return std::nullopt;
}

// Concatenate text from every "type":"text" content block in an end_turn line.
std::string extract_response_text(const std::string& line)
{
    static constexpr std::string_view k_type_text = "\"type\":\"text\"";
    std::string result;
    std::size_t search_from = 0;
    while (true) {
        const auto text_type = line.find(k_type_text, search_from);
        if (text_type == std::string::npos) break;
        search_from = text_type + k_type_text.size();
        if (auto text = extract_json_string(line.substr(text_type), "text")) result += *text;
    }
    return result;
}

std::optional<std::string> wait_for_end_turn(
    const std::string& transcript_path,
    long baseline_offset,
    std::optional<std::chrono::steady_clock::time_point> deadline)
{
    constexpr auto poll_interval = std::chrono::milliseconds{100};
    const auto eff_deadline = deadline.value_or(
        std::chrono::steady_clock::now() + k_response_timeout);

    while (true) {
        if (std::chrono::steady_clock::now() >= eff_deadline) return std::nullopt;
        std::this_thread::sleep_for(poll_interval);

        FILE* fp = fopen(transcript_path.c_str(), "r");  // NOLINT(cppcoreguidelines-owning-memory) -- FILE* closed in this scope
        if (fp == nullptr) continue;
        fseek(fp, baseline_offset, SEEK_SET);

        // Accumulate fgets chunks into a full logical line before matching.
        std::string partial;
        char buf[65536];
        while (fgets(buf, sizeof(buf), fp) != nullptr) {
            partial += buf;
            if (partial.empty() || partial.back() != '\n') continue;  // incomplete line fragment

            if (partial.find("\"type\":\"assistant\"") != std::string::npos &&
                partial.find("\"stop_reason\":\"end_turn\"") != std::string::npos) {
                fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory) -- matches fopen above
                return extract_response_text(partial);
            }
            partial.clear();
        }
        fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory) -- matches fopen above
    }
}

}  // namespace

namespace attractor {

ClaudeCodeTmuxBackend::ClaudeCodeTmuxBackend(std::string tmux_bin)
    : m_tmux_bin{std::move(tmux_bin)}
{}

auto ClaudeCodeTmuxBackend::run(const Node& node, const PromptText& prompt,
                                 Context& /*ctx*/) const
    -> std::expected<LlmResponse, Outcome>
{
    const std::string session = derive_session_name(node);
    const std::string settings_path =
        std::string{ATTRACTOR_CLI_SCRIPTS_DIR} + "/att-tmux-backend.settings.json";

    std::optional<std::chrono::steady_clock::time_point> deadline;
    if (node.timeout) {
        deadline = std::chrono::steady_clock::now() + node.timeout->get_value();
    }

    // Check cache first without holding the lock during blocking I/O (P2).
    std::string transcript_path;
    {
        std::lock_guard lock{m_mutex};
        if (auto it = m_sessions.find(session); it != m_sessions.end()) {
            transcript_path = it->second;
        }
    }

    if (transcript_path.empty()) {
        std::string path;
        if (has_session(m_tmux_bin, session)) {
            path = poll_transcript_path(session, deadline);
        } else {
            path = create_session(m_tmux_bin, session, settings_path, deadline);
        }
        // Re-acquire to update cache; another thread may have established the session.
        std::lock_guard lock{m_mutex};
        if (auto it = m_sessions.find(session); it != m_sessions.end()) {
            transcript_path = it->second;
        } else if (!path.empty()) {
            m_sessions[session] = path;
            transcript_path = path;
        }
    }

    if (transcript_path.empty()) {
        return std::unexpected(
            Outcome::fail(DiagnosticMessage{"tmux: failed to obtain transcript for " + session}));
    }

    // Guard against deadline having been consumed entirely by session establishment (P7).
    if (deadline && std::chrono::steady_clock::now() >= *deadline) {
        return std::unexpected(Outcome::fail(DiagnosticMessage{"timeout"}));
    }

    const long offset = record_offset(transcript_path);

    const std::string cmd = m_tmux_bin + " send-keys -t " + session
        + " -- " + shell_escape(type_safe::get(prompt)) + " Enter";
    if (tmux_system(cmd) != 0) {
        return std::unexpected(
            Outcome::fail(DiagnosticMessage{"tmux: send-keys failed for " + session}));
    }

    auto response = wait_for_end_turn(transcript_path, offset, deadline);
    if (!response) {
        return std::unexpected(Outcome::fail(DiagnosticMessage{"timeout"}));
    }

    return LlmResponse{std::move(*response)};
}

}  // namespace attractor
