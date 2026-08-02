#pragma once

#include <string>
#include <atomic>
#include <vector>

// Result of executing a shell command
struct command_result {
    std::string output;
    int exit_code = 0;
};

// Execute a shell command in a working directory.
// Cross-platform (Windows + Unix).
// Respects interruption via is_interrupted flag.
// Output is truncated to MAX_OUTPUT_LENGTH (keeps tail).
class command_executor {
public:
    static constexpr size_t MAX_OUTPUT_LENGTH = 100000;

    // Execute a command. Returns result with output and exit code.
    // The command is killed if it exceeds timeout_ms (default 120000ms = 2 min).
    static command_result execute(const std::string& command,
                                  const std::string& working_dir,
                                  std::atomic<bool>& is_interrupted,
                                  int timeout_ms = 120000);
};
