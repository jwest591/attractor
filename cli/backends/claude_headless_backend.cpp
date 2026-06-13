#include "claude_headless_backend.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <type_safe/strong_typedef.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <optional>
#include <string>

#include <dirent.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

struct SubprocessResult {
    std::string stdout_data;
    std::string stderr_data;
    int exit_code{0};
    bool timed_out{false};
};

[[nodiscard]] auto run_subprocess(const std::string& exe, const std::string& prompt,
                                  std::optional<std::chrono::milliseconds> timeout)
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

    // Fix: fork() == -1 not handled — waitpid(-1) reaps any child; kill(-1,SIGKILL) is dangerous
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

        // Fix: Close all inherited fds >= 3 (pipe originals + any other parent fds)
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

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        const char* argv[] = {exe.c_str(), "-p", "--output-format", "text", nullptr};
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
            break;
        }

        if ((fds[0].revents & POLLIN) != 0) {
            // Fix: Retry read on EINTR — EINTR must not be treated as EOF
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
            // child closes its read-end; taking the POLLOUT branch would call write() on a
            // broken pipe and raise SIGPIPE, killing the parent with the default handler.
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
    // Fix: Retry waitpid on EINTR — a signal arriving during the wait would otherwise
    // leave the child as a zombie and return with status uninitialized.
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
    if (node.timeout) {
        timeout_ms = node.timeout->get_value();
    }

    auto result = run_subprocess(m_claude_exe, type_safe::get(prompt), timeout_ms);

    if (result.timed_out) {
        return std::unexpected(Outcome::fail(DiagnosticMessage{"timeout"}));
    }

    if (result.exit_code != 0) {
        std::string reason = result.stderr_data.empty()
            ? "claude exited with code " + std::to_string(result.exit_code)
            : result.stderr_data;
        return std::unexpected(Outcome::fail(DiagnosticMessage{std::move(reason)}));
    }

    return LlmResponse{result.stdout_data};
}

}  // namespace attractor
