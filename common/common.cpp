// Various helper functions and utilities (HTTP-only, no llama.cpp/ggml deps)

#include "common.h"
#include "log.h"

#include <algorithm>
#include <cinttypes>
#include <climits>
#include <cmath>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#   define NOMINMAX
#endif
#include <locale>
#include <windows.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(__linux__)
#include <sys/types.h>
#include <pwd.h>
#endif

#if defined(_MSC_VER)
#pragma warning(disable: 4244 4267) // possible loss of data
#endif

// ---------------------------------------------------------------------------
// Time measurement
// ---------------------------------------------------------------------------

static int64_t common_time_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

common_time_meas::common_time_meas(int64_t & t_acc, bool disable)
    : t_start_us(disable ? -1 : common_time_us()), t_acc(t_acc) {}

common_time_meas::~common_time_meas() {
    if (t_start_us >= 0) {
        t_acc += common_time_us() - t_start_us;
    }
}

// ---------------------------------------------------------------------------
// CPU utils
// ---------------------------------------------------------------------------

int32_t common_cpu_get_num_physical_cores() {
#ifdef __linux__
    std::unordered_set<std::string> siblings;
    for (uint32_t cpu = 0; cpu < UINT32_MAX; ++cpu) {
        std::ifstream thread_siblings("/sys/devices/system/cpu/cpu"
            + std::to_string(cpu) + "/topology/thread_siblings");
        if (!thread_siblings.is_open()) {
            break;
        }
        std::string line;
        if (std::getline(thread_siblings, line)) {
            siblings.insert(line);
        }
    }
    if (!siblings.empty()) {
        return static_cast<int32_t>(siblings.size());
    }
    // Fallback if thread siblings file is unavailable
    return static_cast<int32_t>(std::thread::hardware_concurrency());
#elif defined(__APPLE__) && defined(__MACH__)
    int32_t num_physical_cores;
    size_t len = sizeof(num_physical_cores);
    int result = sysctlbyname("hw.physicalcpu", &num_physical_cores, &len, NULL, 0);
    if (result == 0) {
        return num_physical_cores;
    }
    // Fallback if sysctlbyname fails
    return static_cast<int32_t>(std::thread::hardware_concurrency());
#elif defined(_WIN32) && (_WIN32_WINNT >= 0x0601) && !defined(__MINGW64__)
    unsigned int n_threads_win = std::thread::hardware_concurrency();
    unsigned int default_threads = n_threads_win > 0 ? (n_threads_win <= 4 ? n_threads_win : n_threads_win / 2) : 4;
    return static_cast<int32_t>(default_threads);
#else
    return static_cast<int32_t>(std::thread::hardware_concurrency());
#endif
}

int32_t common_cpu_get_num_math() {
    return common_cpu_get_num_physical_cores();
}

void postprocess_cpu_params(common_cpu_params & cpuparams, const common_cpu_params * role_model) {
    if (cpuparams.n_threads < 0) {
        if (role_model != nullptr) {
            cpuparams = *role_model;
        } else {
            cpuparams.n_threads = common_cpu_get_num_math();
        }
    }
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void common_init() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

// ---------------------------------------------------------------------------
// Token helpers (stubs - only used for template initialization)
// ---------------------------------------------------------------------------

std::string common_token_to_piece(const struct llama_vocab * vocab, llama_token token, bool special) {
    // In the HTTP-only build, this is only used for BOS/EOS token extraction
    // from templates. Since we don't have a real model, return a placeholder.
    return "[TOKEN:" + std::to_string(token) + "]";
}

// ---------------------------------------------------------------------------
// File system helpers
// ---------------------------------------------------------------------------

std::vector<common_file_info> fs_list(const std::string & path, bool include_directories) {
    std::vector<common_file_info> files;
    if (path.empty()) return files;

    std::filesystem::path dir(path);
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        return files;
    }

    for (const auto & entry : std::filesystem::directory_iterator(dir)) {
        try {
            const auto & p = entry.path();
            if (std::filesystem::is_regular_file(p)) {
                common_file_info info;
                info.path   = p.string();
                info.name   = p.filename().string();
                info.is_dir = false;
                try {
                    info.size = static_cast<size_t>(std::filesystem::file_size(p));
                } catch (const std::filesystem::filesystem_error &) {
                    info.size = 0;
                }
                files.push_back(std::move(info));
            } else if (include_directories && std::filesystem::is_directory(p)) {
                common_file_info info;
                info.path   = p.string();
                info.name   = p.filename().string();
                info.size   = 0;
                info.is_dir = true;
                files.push_back(std::move(info));
            }
        } catch (const std::filesystem::filesystem_error &) {
            continue;
        }
    }

    return files;
}

std::string string_join(const std::vector<std::string> & values, const std::string & separator) {
    std::string result;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            result += separator;
        }
        result += values[i];
    }
    return result;
}

std::string string_repeat(const std::string & str, size_t count) {
    std::string result;
    for (size_t i = 0; i < count; ++i) {
        result += str;
    }
    return result;
}

std::vector<std::string> string_split(const std::string & str, const std::string & separator) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = str.find(separator);
    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + separator.size();
        end = str.find(separator, start);
    }
    result.push_back(str.substr(start));
    return result;
}

// ---------------------------------------------------------------------------
// TTY utils
// ---------------------------------------------------------------------------

bool tty_can_use_colors() {
    if (const char * no_color = std::getenv("NO_COLOR")) {
        if (no_color[0] != '\0') {
            return false;
        }
    }

    if (const char * term = std::getenv("TERM")) {
        if (std::strcmp(term, "dumb") == 0) {
            return false;
        }
    }

    bool stdout_is_tty = isatty(fileno(stdout));
    bool stderr_is_tty = isatty(fileno(stderr));

    return stdout_is_tty || stderr_is_tty;
}

// ---------------------------------------------------------------------------
// Memory helpers (stub)
// ---------------------------------------------------------------------------

void common_memory_breakdown_print(const struct llama_context * ctx) {
    // No-op in HTTP-only build
    (void)ctx;
}
