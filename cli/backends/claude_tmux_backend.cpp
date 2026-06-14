#include "claude_tmux_backend.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <variant>

#include <cstdio>   // fopen, fclose, fgets, fseek, ftell, popen, pclose
#include <cstdlib>  // system, std::strtol

namespace {

constexpr auto k_session_start_timeout  = std::chrono::seconds{30};
constexpr auto k_response_timeout       = std::chrono::minutes{5};
constexpr int  k_max_rate_limit_retries = 3;

// ---------------------------------------------------------------------------
// TurnResult variant
// ---------------------------------------------------------------------------

struct EndTurn   { std::string text; uint32_t input_tokens{0}; };
struct MaxTokens { std::string text; };
struct ApiError  { std::string error_type; std::string message; };
struct TimedOut  {};

using TurnResult = std::variant<EndTurn, MaxTokens, ApiError, TimedOut>;

// ---------------------------------------------------------------------------
// Low-level helpers
// ---------------------------------------------------------------------------

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

std::string compute_handoff_path(const std::string& session_name)
{
    const auto dir = std::filesystem::current_path() / ".attractor";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / (session_name + "-handoff.md")).string();
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

        FILE* fp = fopen(marker.c_str(), "r");  // NOLINT(cppcoreguidelines-owning-memory)
        if (fp == nullptr) continue;
        char buf[4096] = {};
        fgets(buf, sizeof(buf), fp);
        fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)

        std::string path(buf);
        if (!path.empty() && path.back() == '\n') path.pop_back();
        if (!path.empty()) return path;
    }
}

std::string create_session(
    const std::string& tmux,
    const std::string& name,
    const std::string& settings_path,
    const std::string& handoff_path,
    std::optional<std::chrono::steady_clock::time_point> deadline)
{
    const std::string cmd = tmux + " new-session -d -s " + name
        + " -e " + shell_escape("CLAUDE_HANDOFF_FILE=" + handoff_path)
        + " 'claude -n " + name
        + " --settings " + shell_escape(settings_path)
        + " --dangerously-skip-permissions' 2>/dev/null";
    if (tmux_system(cmd) != 0) return {};
    return poll_transcript_path(name, deadline);
}

long record_offset(const std::string& transcript_path)
{
    FILE* fp = fopen(transcript_path.c_str(), "r");  // NOLINT(cppcoreguidelines-owning-memory)
    if (fp == nullptr) return 0L;
    fseek(fp, 0L, SEEK_END);
    long off = ftell(fp);
    fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)
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

uint32_t extract_input_tokens(const std::string& line)
{
    constexpr std::string_view needle = "\"input_tokens\":";
    const auto pos = line.find(needle);
    if (pos == std::string::npos) return 0;
    uint32_t result = 0;
    for (std::size_t i = pos + needle.size(); i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])); ++i)
        result = result * 10 + static_cast<uint32_t>(line[i] - '0');
    return result;
}

std::string extract_error_type(const std::string& line)
{
    const auto pos = line.find("\"error\":{");
    if (pos == std::string::npos) return {};
    return extract_json_string(line.substr(pos), "type").value_or(std::string{});
}

std::string extract_error_message(const std::string& line)
{
    const auto pos = line.find("\"error\":{");
    if (pos == std::string::npos) return {};
    return extract_json_string(line.substr(pos), "message").value_or(std::string{});
}

// ---------------------------------------------------------------------------
// wait_for_end_turn — returns TurnResult
// ---------------------------------------------------------------------------

TurnResult wait_for_end_turn(
    const std::string& transcript_path,
    long baseline_offset,
    std::optional<std::chrono::steady_clock::time_point> deadline)
{
    constexpr auto poll_interval = std::chrono::milliseconds{100};
    const auto eff_deadline = deadline.value_or(
        std::chrono::steady_clock::now() + k_response_timeout);

    while (true) {
        if (std::chrono::steady_clock::now() >= eff_deadline) return TimedOut{};
        std::this_thread::sleep_for(poll_interval);

        FILE* fp = fopen(transcript_path.c_str(), "r");  // NOLINT(cppcoreguidelines-owning-memory)
        if (fp == nullptr) continue;
        fseek(fp, baseline_offset, SEEK_SET);

        std::string partial;
        char buf[65536];
        while (fgets(buf, sizeof(buf), fp) != nullptr) {
            partial += buf;
            if (partial.empty() || partial.back() != '\n') continue;

            if (partial.find("\"type\":\"assistant\"") != std::string::npos) {
                if (partial.find("\"stop_reason\":\"end_turn\"") != std::string::npos) {
                    fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)
                    return EndTurn{extract_response_text(partial), extract_input_tokens(partial)};
                }
                if (partial.find("\"stop_reason\":\"max_tokens\"") != std::string::npos) {
                    fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)
                    return MaxTokens{extract_response_text(partial)};
                }
            } else if (partial.find("\"type\":\"error\"") != std::string::npos) {
                fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)
                return ApiError{extract_error_type(partial), extract_error_message(partial)};
            }
            partial.clear();
        }
        fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)
    }
}

// ---------------------------------------------------------------------------
// /usage polling and reset-time parsing (Tasks 2.1 – 2.4)
// ---------------------------------------------------------------------------

std::optional<std::string> poll_usage_stdout(
    const std::string& transcript_path,
    long baseline_offset,
    std::optional<std::chrono::steady_clock::time_point> deadline)
{
    constexpr auto poll_interval = std::chrono::milliseconds{200};
    const auto eff_deadline = deadline.value_or(
        std::chrono::steady_clock::now() + std::chrono::seconds{30});

    while (true) {
        if (std::chrono::steady_clock::now() >= eff_deadline) return std::nullopt;
        std::this_thread::sleep_for(poll_interval);

        FILE* fp = fopen(transcript_path.c_str(), "r");  // NOLINT(cppcoreguidelines-owning-memory)
        if (fp == nullptr) continue;
        fseek(fp, baseline_offset, SEEK_SET);

        std::string partial;
        char buf[65536];
        while (fgets(buf, sizeof(buf), fp) != nullptr) {
            partial += buf;
            if (partial.empty() || partial.back() != '\n') continue;

            if (partial.find("\"subtype\":\"local_command\"") != std::string::npos
                && partial.find("<local-command-stdout>") != std::string::npos
                && partial.find("resets") != std::string::npos) {
                fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)
                const auto start = partial.find("<local-command-stdout>");
                const auto end   = partial.find("</local-command-stdout>", start);
                if (start == std::string::npos || end == std::string::npos)
                    return std::nullopt;
                constexpr std::size_t tag_len = 22;  // len("<local-command-stdout>")
                return partial.substr(start + tag_len, end - start - tag_len);
            }
            partial.clear();
        }
        fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)
    }
}

std::string strip_ansi(const std::string& s)
{
    static const std::regex k_ansi{"\x1b(?:\\[[0-9;]*[A-Za-z]|\\][^\x07]*(?:\x07|\x1b\\\\))"};
    return std::regex_replace(s, k_ansi, "");
}

std::optional<std::chrono::seconds> parse_reset_duration(const std::string& usage_stdout)
{
    const std::string clean = strip_ansi(usage_stdout);
    std::istringstream ss(clean);
    std::string line;
    while (std::getline(ss, line)) {
        const auto session_pos = line.find("Current session:");
        if (session_pos == std::string::npos) continue;
        const auto resets_pos = line.find("resets ", session_pos);
        if (resets_pos == std::string::npos) continue;
        std::string reset_str = line.substr(resets_pos + 7);
        if (const auto paren = reset_str.find('('); paren != std::string::npos)
            reset_str = reset_str.substr(0, paren);
        while (!reset_str.empty() && std::isspace(static_cast<unsigned char>(reset_str.back())))
            reset_str.pop_back();
        reset_str.erase(std::remove(reset_str.begin(), reset_str.end(), ','), reset_str.end());
        reset_str.erase(std::remove(reset_str.begin(), reset_str.end(), '\''), reset_str.end());

        const std::string cmd = "date -d '" + reset_str + "' +%s 2>/dev/null";
        // NOLINTNEXTLINE(cert-env33-c) -- controlled input, no user data in cmd
        FILE* fp = popen(cmd.c_str(), "r");
        if (fp == nullptr) return std::nullopt;
        char buf[32] = {};
        fgets(buf, sizeof(buf), fp);
        pclose(fp);

        const long target_ts = std::strtol(buf, nullptr, 10);
        if (target_ts <= 0) return std::nullopt;
        const auto now_ts = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return std::chrono::seconds{std::max(0L, target_ts - now_ts)};
    }
    return std::nullopt;
}

std::optional<std::chrono::seconds> send_usage_and_wait(
    const std::string& tmux_bin,
    const std::string& session,
    const std::string& transcript_path,
    std::optional<std::chrono::steady_clock::time_point> deadline)
{
    const long offset = record_offset(transcript_path);
    const std::string cmd = tmux_bin + " send-keys -t " + session + " -- /usage Enter";
    if (tmux_system(cmd) != 0) return std::nullopt;
    auto stdout_text = poll_usage_stdout(transcript_path, offset, deadline);
    if (!stdout_text) return std::nullopt;
    return parse_reset_duration(*stdout_text);
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
    const std::string handoff_path = compute_handoff_path(session);
    { std::error_code ec; std::filesystem::remove(std::filesystem::path(handoff_path), ec); }

    std::optional<std::chrono::steady_clock::time_point> deadline;
    if (node.timeout) {
        deadline = std::chrono::steady_clock::now() + node.timeout->get_value();
    }

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
            path = create_session(m_tmux_bin, session, settings_path, handoff_path, deadline);
        }
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

    if (deadline && std::chrono::steady_clock::now() >= *deadline) {
        return std::unexpected(Outcome::fail(DiagnosticMessage{"timeout"}));
    }

    std::optional<std::chrono::seconds> last_rate_limit_reset;
    for (int attempt = 0; attempt <= k_max_rate_limit_retries; ++attempt) {
        const long offset = record_offset(transcript_path);

        const std::string send_cmd = m_tmux_bin + " send-keys -t " + session
            + " -- " + shell_escape(type_safe::get(prompt)) + " Enter";
        if (tmux_system(send_cmd) != 0) {
            return std::unexpected(
                Outcome::fail(DiagnosticMessage{"tmux: send-keys failed for " + session}));
        }

        auto result = wait_for_end_turn(transcript_path, offset, deadline);

        if (std::holds_alternative<EndTurn>(result)) {
            auto& et = std::get<EndTurn>(result);
            if (et.text.empty()) {
                return std::unexpected(Outcome::fail(
                    DiagnosticMessage{"tmux: assistant turn contained no text content"}));
            }
            return LlmResponse{std::move(et.text)};
        }
        if (std::holds_alternative<MaxTokens>(result)) {
            return std::unexpected(Outcome::fail(
                DiagnosticMessage{"tmux: response truncated (max_tokens) -- reduce prompt size or increase max_tokens config"}));
        }
        if (std::holds_alternative<TimedOut>(result)) {
            return std::unexpected(Outcome::fail(DiagnosticMessage{"timeout"}));
        }

        auto& err = std::get<ApiError>(result);
        if (err.error_type != "rate_limit_error") {
            return std::unexpected(Outcome::fail(
                DiagnosticMessage{"tmux: API error [" + err.error_type + "]: " + err.message}));
        }

        auto wait_duration = send_usage_and_wait(m_tmux_bin, session, transcript_path, deadline);
        if (wait_duration) {
            last_rate_limit_reset = *wait_duration;
            const auto sleep_until = std::chrono::steady_clock::now() + *wait_duration
                + std::chrono::seconds{5};
            if (!deadline || sleep_until < *deadline) {
                std::this_thread::sleep_until(sleep_until);
            }
        }

        if (attempt == k_max_rate_limit_retries) {
            std::string reset_note = last_rate_limit_reset.has_value()
                ? "; last parsed reset wait was " + std::to_string(last_rate_limit_reset->count()) + "s"
                : "";
            return std::unexpected(Outcome::fail(DiagnosticMessage{
                "tmux: rate_limit_error: max retries (" +
                std::to_string(k_max_rate_limit_retries) + ") exhausted" + reset_note}));
        }
    }
    __builtin_unreachable();  // loop always returns on final attempt
}

}  // namespace attractor
