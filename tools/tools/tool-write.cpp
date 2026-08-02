#include "../tool-registry.h"
#include "../permission.h"

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Security: Path validation — prevent path traversal attacks
// ---------------------------------------------------------------------------
static bool is_path_in_dir(const std::string & file_path, const std::string & working_dir) {
    try {
        // Resolve the working directory to its absolute canonical path
        fs::path abs_workdir = fs::absolute(fs::canonical(working_dir));

        // Resolve the target file path
        fs::path abs_file = fs::absolute(file_path);

        // If the file exists, resolve ALL symlinks (including the final component)
        // to prevent symlink-based path traversal (e.g. working_dir/creds -> /etc/passwd).
        if (fs::exists(abs_file)) {
            abs_file = fs::canonical(abs_file);
        }
        // If the file doesn't exist yet, resolve the parent directory and append
        // the filename, so symlinks in the directory chain are also resolved.
        else if (abs_file.has_parent_path() && fs::exists(abs_file.parent_path())) {
            abs_file = fs::canonical(abs_file.parent_path()) / abs_file.filename();
        }

        std::string file_str = abs_file.string();
        std::string workdir_str = abs_workdir.string();

        // Ensure exact match or proper prefix (with trailing slash)
        if (file_str == workdir_str) {
            return true;
        }
        if (file_str.size() > workdir_str.size() &&
            file_str.substr(0, workdir_str.size() + 1) == workdir_str + "/") {
            return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

static tool_result write_execute(const json & args, const tool_context & ctx) {
    std::string file_path = args.value("file_path", "");
    std::string content = args.value("content", "");

    if (file_path.empty()) {
        return {false, "", "file_path parameter is required"};
    }

    // Make absolute if relative
    fs::path path(file_path);
    if (path.is_relative()) {
        path = fs::path(ctx.working_dir) / path;
    }

    // Security: Prevent path traversal — file must be inside working_dir
    if (!is_path_in_dir(path.string(), ctx.working_dir)) {
        return {false, "", "Path traversal detected: file must be within the working directory"};
    }

    // Block sensitive files
    if (permission_manager::is_sensitive_file(path.string())) {
        return {false, "", "Cannot write to sensitive file (contains credentials/secrets): " + path.string()};
    }

    // Check if file exists (for reporting)
    bool existed = fs::exists(path);

    // Create parent directories if needed
    if (path.has_parent_path()) {
        try {
            fs::create_directories(path.parent_path());
        } catch (const fs::filesystem_error & e) {
            return {false, "", "Failed to create directories: " + std::string(e.what())};
        }
    }

    // Write file
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {false, "", "Cannot open file for writing: " + path.string()};
    }

    file << content;
    file.close();

    if (file.fail()) {
        return {false, "", "Error writing to file: " + path.string()};
    }

    std::string msg = existed ? "File updated: " : "File created: ";
    msg += path.string();
    msg += " (" + std::to_string(content.length()) + " bytes)";

    return {true, msg, ""};
}

static tool_def write_tool = {
    "write",
    "Create a new file or overwrite an existing file with the given content.",
    R"json({
        "type": "object",
        "properties": {
            "file_path": {
                "type": "string",
                "description": "Path to the file to write (absolute or relative to working directory)"
            },
            "content": {
                "type": "string",
                "description": "The content to write to the file"
            }
        },
        "required": ["file_path", "content"]
    })json",
    write_execute
};

REGISTER_TOOL(write, write_tool);
