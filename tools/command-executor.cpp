#include "command-executor.h"

#include <cstdio>
#include <cstring>
#include <chrono>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#   define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

command_result command_executor::execute(const std::string& command,
                                         const std::string& working_dir,
                                         std::atomic<bool>& is_interrupted,
                                         int timeout_ms) {
    command_result result;
    result.exit_code = 0;
    bool timed_out = false;

#ifdef _WIN32
    // -------------------------------------------------------------------------
    // Windows implementation
    // -------------------------------------------------------------------------
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        result.output = "[Failed to create pipe]\n";
        result.exit_code = 1;
        return result;
    }

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {sizeof(STARTUPINFOA)};
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    std::string cmd_line = "cmd /c " + command;

    if (!CreateProcessA(nullptr, (LPSTR)cmd_line.c_str(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, working_dir.c_str(), &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        result.output = "[Failed to create process]\n";
        result.exit_code = 1;
        return result;
    }

    CloseHandle(hWritePipe);

    char buffer[4096];
    DWORD bytesRead;
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        if (timeout_ms > 0 && elapsed > timeout_ms) {
            TerminateProcess(pi.hProcess, 1);
            timed_out = true;
            break;
        }

        if (is_interrupted.load()) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }

        DWORD available = 0;
        PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &available, nullptr);
        if (available == 0) {
            DWORD wait_result = WaitForSingleObject(pi.hProcess, 100);
            if (wait_result == WAIT_OBJECT_0) break;
            continue;
        }

        if (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            fwrite(buffer, 1, bytesRead, stdout);
            fflush(stdout);
            result.output.append(buffer, bytesRead);
            if (result.output.size() > MAX_OUTPUT_LENGTH * 2) {
                result.output.erase(0, result.output.size() - MAX_OUTPUT_LENGTH);
            }
        }
    }

    DWORD exitCodeDword;
    GetExitCodeProcess(pi.hProcess, &exitCodeDword);
    result.exit_code = (int)exitCodeDword;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

#else
    // -------------------------------------------------------------------------
    // Unix/POSIX implementation
    // -------------------------------------------------------------------------
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        result.output = "[Failed to create pipe]\n";
        result.exit_code = 1;
        return result;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        result.output = "[Failed to fork process]\n";
        result.exit_code = 1;
        return result;
    }

    if (pid == 0) {
        // Child process
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);

        if (chdir(working_dir.c_str()) != 0) {
            _exit(127);
        }

        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    // Parent process
    close(pipe_fd[1]);

    // Set non-blocking read
    int flags = fcntl(pipe_fd[0], F_GETFL, 0);
    fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);

    char buffer[4096];
    bool child_reaped = false;
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        if (timeout_ms > 0 && elapsed > timeout_ms) {
            kill(pid, SIGKILL);
            timed_out = true;
            break;
        }

        if (is_interrupted.load()) {
            kill(pid, SIGKILL);
            break;
        }

        ssize_t n = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            fwrite(buffer, 1, n, stdout);
            fflush(stdout);
            result.output.append(buffer, n);
            if (result.output.size() > MAX_OUTPUT_LENGTH * 2) {
                result.output.erase(0, result.output.size() - MAX_OUTPUT_LENGTH);
            }
        } else if (n == 0) {
            // EOF
            break;
        } else {
            // EAGAIN - no data available
            int status;
            pid_t wp = waitpid(pid, &status, WNOHANG);
            if (wp == pid) {
                // Process ended, read remaining data
                while ((n = read(pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) {
                    buffer[n] = '\0';
                    fwrite(buffer, 1, n, stdout);
                    fflush(stdout);
                    result.output.append(buffer, n);
                    if (result.output.size() > MAX_OUTPUT_LENGTH * 2) {
                        result.output.erase(0, result.output.size() - MAX_OUTPUT_LENGTH);
                    }
                }
                result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                child_reaped = true;
                break;
            }
            usleep(10000);  // 10ms
        }
    }

    close(pipe_fd[0]);

    // Wait for child if not already done
    if (!child_reaped) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            result.exit_code = 128 + WTERMSIG(status);
        }
    }
#endif

    // Truncate to max output length (keep tail)
    if (result.output.size() > MAX_OUTPUT_LENGTH) {
        result.output = result.output.substr(result.output.size() - MAX_OUTPUT_LENGTH);
        size_t nl = result.output.find('\n');
        if (nl != std::string::npos && nl < 200) {
            result.output = result.output.substr(nl + 1);
        }
        result.output = "[output truncated]\n" + result.output;
    }

    if (timed_out) {
        result.output += "\n[Timed out after " + std::to_string(timeout_ms) + "ms]";
        result.exit_code = 1;
    }

    return result;
}