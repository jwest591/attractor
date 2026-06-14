#include "claude_headless_backend.hpp"
#include "backend_utils.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <climits>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include <cstdlib>
#include <dirent.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

// ---- JSON helpers (minimal, duplicated from claude_tmux_backend.cpp) --------

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

// ---- Stream-JSON parsing ----------------------------------------------------

struct HeadlessStreamResult {
    std::string text;
    std::string stop_reason;
    std::string error_type;
    std::string error_message;
};

HeadlessStreamResult parse_stream_json(const std::string& stdout_data)
{
    HeadlessStreamResult result;
    std::istringstream ss(stdout_data);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find("\"type\":\"content_block_delta\"") != std::string::npos
            && line.find("\"type\":\"text_delta\"") != std::string::npos) {
            if (auto t = extract_json_string(line, "text")) result.text += *t;
        } else if (line.find("\"type\":\"message_delta\"") != std::string::npos) {
            if (auto r = extract_json_string(line, "stop_reason")) result.stop_reason = *r;
        } else if (line.find("\"type\":\"error\"") != std::string::npos) {
            result.error_type    = extract_error_type(line);
            result.error_message = extract_error_message(line);
        }
    }
    return result;
}

// ---- Subprocess -------------------------------------------------------------

struct SubprocessResult {
    std::string stdout_data;
    std::string stderr_data;
    int exit_code{0};
    bool timed_out{false};
};

[[nodiscard]] auto run_subprocess(const std::string& exe, const std::string& prompt,
                                  std::optional<std::chrono::milliseconds> timeout,
                                  const std::string& handoff_path)
    -> SubprocessResult
{
    int stdin_fd[2]  = {-1, -1};
    int stdout_fd[2] = {-1, -1};
    int stderr_fd[2] = {-1, -1};

    // Fix: Check pipe() return values; close any that succeeded on partial failure
    if (pipe(stdin_fd) < 0 || pipe(stdout_fd) < 0 || pipe(stderr_fd) < 0) {
        for (int fd : {stdin_fd[0], stdin_fd[1], stdout_fd[0], stdout_fd[1], stderr_fd[0], stderr_fd[1]}) {
            if (fd >= 0) close(fd);
        }
        SubprocessResult r;
        r.exit_code = -1;
        return r;
    }

    pid_t pid = fork();

    // Fix: fork() == -1 not handled
    if (pid < 0) {
        for (int fd : {stdin_fd[0], stdin_fd[1], stdout_fd[0], stdout_fd[1], stderr_fd[0], stderr_fd[1]}) {
            close(fd);
        }
        SubprocessResult r;
        r.exit_code = -1;
        return r;
    }

    if (pid == 0) {
        dup2(stdin_fd[0], STDIN_FILENO);
        dup2(stdout_fd[1], STDOUT_FILENO);
        dup2(stderr_fd[1], STDERR_FILENO);

        // Fix: Close all inherited fds >= 3
        if (DIR* dirp = opendir("/proc/self/fd"); dirp != nullptr) {
            int dir_fd = dirfd(dirp);
            while (const auto* ent = readdir(dirp)) {
                const char* name = ent->d_name;
                if (*name < '0' || *name > '9') continue;
                int fd = 0;
                while (*name >= '0' && *name <= '9') { fd = fd * 10 + (*name++ - '0'); }
                if (fd > 2 && fd != dir_fd) close(fd);
            }
            closedir(dirp);
        }

        // NOLINTNEXTLINE(concurrency-mt-unsafe) -- child process only
        setenv("CLAUDE_HANDOFF_FILE", handoff_path.c_str(), 1);

        const std::string settings_path =
            std::string{ATTRACTOR_CLI_SCRIPTS_DIR} + "/att-headless-backend.settings.json";
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        const char* argv[] = {
            exe.c_str(), "-p",
            "--output-format", "stream-json",
            "--settings", settings_path.c_str(),
            nullptr
        };
        execvp(exe.c_str(), const_cast<char**>(argv));
        _exit(127);
    }

    // Parent: close child-side pipe ends
    close(stdin_fd[0]);  stdin_fd[0] = -1;
    close(stdout_fd[1]); stdout_fd[1] = -1;
    close(stderr_fd[1]); stderr_fd[1] = -1;

    SubprocessResult result;
    std::array<char, 4096> buf{};
    bool stdout_open = true;
    bool stderr_open = true;

    // Fix: Integrate stdin writes into poll loop to avoid deadlock on prompts > pipe buffer
    const char* write_ptr = prompt.data();
    std::size_t write_remaining = prompt.size();
    bool stdin_writing = (write_remaining > 0);
    if (!stdin_writing) {
        close(stdin_fd[1]);
        stdin_fd[1] = -1;
    }

    auto deadline = timeout
        ? std::optional{std::chrono::steady_clock::now() + *timeout}
        : std::optional<std::chrono::time_point<std::chrono::steady_clock>>{};

    while (stdout_open || stderr_open || stdin_writing) {
        // Once child has closed its output pipes it has exited; writing stdin is pointless
        if (!stdout_open && !stderr_open) {
            if (stdin_fd[1] >= 0) { close(stdin_fd[1]); stdin_fd[1] = -1; }
            stdin_writing = false;
            break;
        }

        struct pollfd fds[3] = {};
        int nfds = 0;
        fds[nfds++] = {stdout_fd[0], POLLIN, 0};
        fds[nfds++] = {stderr_fd[0], POLLIN, 0};
        int stdin_slot = -1;
        if (stdin_writing) {
            stdin_slot = nfds;
            fds[nfds++] = {stdin_fd[1], POLLOUT, 0};
        }

        int poll_ms = -1;
        if (deadline) {
            auto now = std::chrono::steady_clock::now();
            if (now >= *deadline) { result.timed_out = true; break; }
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - now);
            // Fix: Clamp before narrowing cast — large timeout wraps to negative (infinite wait)
            poll_ms = static_cast<int>(std::min(ms.count(), static_cast<int64_t>(INT_MAX)));
        }

        // Fix: Retry poll on EINTR
        int ret = poll(fds, static_cast<nfds_t>(nfds), poll_ms);
        if (ret == 0) { result.timed_out = true; break; }
        if (ret < 0) {
            if (errno == EINTR) continue;
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            break;
        }

        if ((fds[0].revents & POLLIN) != 0) {
            // Fix: Retry read on EINTR
            ssize_t n;
            do { n = read(stdout_fd[0], buf.data(), buf.size()); } while (n < 0 && errno == EINTR);
            if (n > 0) result.stdout_data.append(buf.data(), static_cast<std::size_t>(n));
            else stdout_open = false;
        } else if ((fds[0].revents & (POLLHUP | POLLERR)) != 0) {
            for (;;) {
                ssize_t n;
                do { n = read(stdout_fd[0], buf.data(), buf.size()); } while (n < 0 && errno == EINTR);
                if (n <= 0) break;
                result.stdout_data.append(buf.data(), static_cast<std::size_t>(n));
            }
            stdout_open = false;
        }

        if ((fds[1].revents & POLLIN) != 0) {
            ssize_t n;
            do { n = read(stderr_fd[0], buf.data(), buf.size()); } while (n < 0 && errno == EINTR);
            if (n > 0) result.stderr_data.append(buf.data(), static_cast<std::size_t>(n));
            else stderr_open = false;
        } else if ((fds[1].revents & (POLLHUP | POLLERR)) != 0) {
            for (;;) {
                ssize_t n;
                do { n = read(stderr_fd[0], buf.data(), buf.size()); } while (n < 0 && errno == EINTR);
                if (n <= 0) break;
                result.stderr_data.append(buf.data(), static_cast<std::size_t>(n));
            }
            stderr_open = false;
        }

        if (stdin_slot >= 0) {
            // Fix: Check POLLERR before POLLOUT — both can be set simultaneously when the
            // child closes its read-end; taking POLLOUT branch calls write() on a broken pipe.
            if ((fds[stdin_slot].revents & POLLERR) != 0) {
                close(stdin_fd[1]);
                stdin_fd[1] = -1;
                stdin_writing = false;
            } else if ((fds[stdin_slot].revents & POLLOUT) != 0) {
                ssize_t n;
                do { n = write(stdin_fd[1], write_ptr, write_remaining); } while (n < 0 && errno == EINTR);
                if (n > 0) {
                    write_ptr += n;
                    write_remaining -= static_cast<std::size_t>(n);
                }
                if (write_remaining == 0 || n < 0) {
                    close(stdin_fd[1]);
                    stdin_fd[1] = -1;
                    stdin_writing = false;
                }
            }
        }
    }

    if (stdin_fd[1] >= 0) { close(stdin_fd[1]); }

    if (result.timed_out) { kill(pid, SIGKILL); }

    close(stdout_fd[0]);
    close(stderr_fd[0]);

    int status = 0;
    // Fix: Retry waitpid on EINTR
    { int wp; do { wp = waitpid(pid, &status, 0); } while (wp < 0 && errno == EINTR); }
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return result;
}

}  // namespace

namespace attractor {

ClaudeCodeHeadlessBackend::ClaudeCodeHeadlessBackend(std::string claude_exe)
    : m_claude_exe{std::move(claude_exe)}
{}

auto ClaudeCodeHeadlessBackend::run(const Node& node, const PromptText& prompt,
                                     Context& /*ctx*/) const
    -> std::expected<LlmResponse, Outcome>
{
    std::optional<std::chrono::milliseconds> timeout_ms;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    if (node.timeout) {
        timeout_ms = node.timeout->get_value();
        deadline = std::chrono::steady_clock::now() + *timeout_ms;
    }

    const std::string session_name = derive_session_name(node);
    auto handoff_path_result = compute_handoff_path(session_name);
    if (!handoff_path_result) {
        return std::unexpected(Outcome::fail(DiagnosticMessage{
            "headless: cannot create .attractor directory: " + handoff_path_result.error()}));
    }
    const std::string& handoff_path = *handoff_path_result;
    { std::error_code ec; std::filesystem::remove(std::filesystem::path(handoff_path), ec); }

    constexpr int k_max_retries = 3;
    for (int attempt = 0; attempt <= k_max_retries; ++attempt) {
        auto result = run_subprocess(m_claude_exe, type_safe::get(prompt),
                                     timeout_ms, handoff_path);

        if (result.timed_out) {
            return std::unexpected(Outcome::fail(DiagnosticMessage{"timeout"}));
        }

        if (result.exit_code != 0) {
            std::string reason = result.stderr_data.empty()
                ? "claude exited with code " + std::to_string(result.exit_code)
                : result.stderr_data;
            return std::unexpected(Outcome::fail(DiagnosticMessage{std::move(reason)}));
        }

        auto parsed = parse_stream_json(result.stdout_data);

        if (parsed.stop_reason == "end_turn") {
            return LlmResponse{std::move(parsed.text)};
        }

        if (parsed.stop_reason == "max_tokens") {
            return std::unexpected(Outcome::fail(DiagnosticMessage{
                "headless: response truncated (max_tokens)"}));
        }

        if (!parsed.error_type.empty()) {
            if (parsed.error_type != "rate_limit_error" || attempt == k_max_retries) {
                std::string msg = parsed.error_type == "rate_limit_error"
                    ? "headless: rate_limit_error: max retries (" + std::to_string(k_max_retries)
                      + ") exhausted: " + parsed.error_message
                    : "headless: API error [" + parsed.error_type + "]: " + parsed.error_message;
                return std::unexpected(Outcome::fail(DiagnosticMessage{std::move(msg)}));
            }
            const auto wake = std::chrono::steady_clock::now() + std::chrono::seconds{10};
            if (!deadline || wake < *deadline) {
                std::this_thread::sleep_until(wake);
            }
            continue;
        }

        // stdout non-empty but no recognisable stop_reason -- treat as success
        return LlmResponse{std::move(parsed.text)};
    }
    __builtin_unreachable();  // loop always returns on final attempt
}

}  // namespace attractor
