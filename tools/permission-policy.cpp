#include "permission.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

permission_policy::permission_policy() {
    defaults_[permission_type::BASH]       = permission_state::ASK;
    defaults_[permission_type::FILE_READ]  = permission_state::ALLOW;
    defaults_[permission_type::FILE_WRITE] = permission_state::ASK;
    defaults_[permission_type::FILE_EDIT]  = permission_state::ASK;
    defaults_[permission_type::GLOB]       = permission_state::ALLOW;
    defaults_[permission_type::EXTERNAL_DIR] = permission_state::ASK;
    defaults_[permission_type::MCP_TOOL]   = permission_state::ASK;

    dangerous_patterns_ = {
        "rm -rf", "rm -r /", "rm -f", "rmdir",
        "rm -R", "rm -fr", "rm -rf /", "rm --recursive",
        "sudo ", "su -", "doas ",
        "chmod 777", "chmod -R", "chown -R", "chmod 000",
        "curl | sh", "curl | bash", "wget | sh", "wget | bash",
        "curl -s | sh", "wget -O - |",
        "curl -fsSL", "curl |sudo", "wget -qO- | sh",
        "> /dev/", "dd if=", "mkfs.", ":(){:|:&};:",
        "pip install", "pip3 install", "npm i -g", "npm install -g",
        "brew install", "apt install", "apt-get install", "yum install",
        "git push -f", "git push --force", "git reset --hard",
        "kill -9", "killall", "pkill",
        "shutdown", "reboot", "halt",
        "mount ", "umount ", "fdisk ", "parted ",
        ":(){", "fork bomb", ">|",
        "eval ", "source /etc", ". /etc",
        "systemctl ", "service ", "init ",
        "chattr", "setfacl",
        "nc -e", "ncat -e", "telnet ", "socat ",
        "mknod", "mkfifo",
        "perl -e", "python -c", "python3 -c",
        "base64 -d |", "openssl enc -d",
    };

    safe_patterns_ = {
        "ls", "pwd", "cat ", "head ", "tail ",
        "grep ", "find ", "wc ", "diff ",
        "git status", "git log", "git diff", "git branch",
        "echo ", "which ", "type ", "file "
    };
}

void permission_policy::set_project_root(const std::string & path) {
    project_root_ = fs::absolute(path).string();
}

permission_state permission_policy::classify(
        const permission_request & request,
        bool yolo_mode,
        const std::map<std::string, permission_state> & session_overrides) const {
    if (yolo_mode) {
        return permission_state::ALLOW;
    }

    const std::string key = permission_override_key(request.tool_name, request.details);
    auto it = session_overrides.find(key);
    if (it != session_overrides.end()) {
        return it->second;
    }

    if (request.type == permission_type::BASH) {
        // Normalize shell obfuscation: collapse whitespace, remove backslash escapes
        std::string normalized = normalize_shell_command(request.details);
        if (is_dangerous_bash_command(normalized)) {
            return permission_state::ASK;
        }
        // Even "safe" commands (cat, grep, head, ...) must not auto-allow when
        // they reference paths outside the project (e.g. /etc/passwd, ~/.ssh/).
        if (!is_compound_command(normalized) && matches_pattern(normalized, safe_patterns_)) {
            if (!contains_sensitive_path_reference(normalized)) {
                return permission_state::ALLOW;
            }
        }
    }

    auto def_it = defaults_.find(request.type);
    if (def_it != defaults_.end()) {
        return def_it->second;
    }

    return permission_state::ASK;
}

bool permission_policy::is_external_path(const std::string & path) const {
    return !is_path_in_project(path);
}

bool permission_policy::is_dangerous_bash_command(const std::string & cmd) const {
    // Normalize first to defeat trivial obfuscation (backslash escapes, ${IFS},
    // whitespace runs). Then substring match so dangerous fragments embedded in
    // compound commands (e.g. "cd build && rm -rf /") are still flagged.
    std::string normalized = normalize_shell_command(cmd);
    return contains_pattern(normalized, dangerous_patterns_);
}

// Normalize a shell command to defeat trivial obfuscation:
//   - collapse runs of whitespace to single spaces
//   - remove backslash escapes (e.g. r\m -rf -> rm -rf)
//   - replace ${IFS} with space
//   - strip excessive whitespace around operators
std::string permission_policy::normalize_shell_command(const std::string & cmd) {
    std::string result;
    result.reserve(cmd.size());

    // First pass: handle backslash escapes and ${IFS}
    for (size_t i = 0; i < cmd.size(); i++) {
        char c = cmd[i];
        if (c == '\\' && i + 1 < cmd.size()) {
            // Backslash escape: keep the escaped char (rm \-rf -> rm -rf)
            // But preserve special cases like \n, \t (map to space)
            char next = cmd[i + 1];
            if (next == 'n' || next == 't' || next == 'r') {
                result += ' ';
                i++;
            } else {
                result += next;
                i++;
            }
        } else if (c == '$' && i + 4 < cmd.size() &&
                   cmd[i + 1] == '{' && cmd[i + 2] == 'I' &&
                   cmd[i + 3] == 'F' && cmd[i + 4] == 'S' &&
                   cmd[i + 5] == '}') {
            // ${IFS} -> space
            result += ' ';
            i += 5;
        } else {
            result += c;
        }
    }

    // Second pass: collapse whitespace runs
    std::string collapsed;
    collapsed.reserve(result.size());
    bool in_space = false;
    for (char c : result) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            in_space = true;
        } else {
            if (in_space && !collapsed.empty()) {
                collapsed += ' ';
            }
            in_space = false;
            collapsed += c;
        }
    }

    return collapsed;
}

bool permission_policy::contains_sensitive_path_reference(const std::string & cmd) {
    // Paths that indicate access to system/security-sensitive files.
    // "Safe" commands (cat, grep, head, tail, ...) must not auto-allow these.
    static const std::vector<std::string> sensitive_refs = {
        "/etc/", "/var/", "/usr/", "/bin/", "/sbin/", "/boot/",
        "/root/", "/home/", "/proc/", "/sys/", "/dev/",
        "~/.ssh/", "/etc/passwd", "/etc/shadow", "/etc/sudoers",
        "/etc/group", "/etc/hosts", "/etc/cron", "/etc/systemd",
        "/var/log/", "/var/mail/", "/var/spool/",
        "/usr/bin/", "/usr/sbin/", "/usr/share/",
        "~/.aws/", "~/.config/", "~/.gitconfig", "~/.netrc",
        "~/.bashrc", "~/.profile", "~/.zshrc", "~/.vimrc",
        "~/.gnupg/", "~/.password-store/", "~/Library/",
        "id_rsa", "id_ed25519", ".pem", ".key",
        "/private/", "/Users/", "/System/",
    };

    for (const auto & ref : sensitive_refs) {
        if (cmd.find(ref) != std::string::npos) {
            return true;
        }
    }

    // Tilde expansion to home or direct path to home
    if (cmd.find("~/") != std::string::npos || cmd.find("~ ") != std::string::npos) {
        return true;
    }

    return false;
}

bool permission_policy::is_compound_command(const std::string & cmd) {
    // Detect shell operators that indicate multiple commands or command
    // substitution — these must never be auto-allowed.
    for (const auto & sep : {"|", "&&", "||", ";"}) {
        if (cmd.find(sep) != std::string::npos) {
            return true;
        }
    }

    // Command substitution: $(...), `...`, ${...} with command execution
    if (cmd.find("$(") != std::string::npos) {
        return true;
    }
    if (cmd.find('`') != std::string::npos) {
        return true;
    }

    // Redirection and pipes
    if (cmd.find('>') != std::string::npos || cmd.find('<') != std::string::npos) {
        return true;
    }

    // Newlines (multi-line commands)
    if (cmd.find('\n') != std::string::npos) {
        return true;
    }

    // Backgrounding
    if (cmd.find("&") != std::string::npos) {
        return true;
    }

    return false;
}

bool permission_policy::matches_pattern(const std::string & cmd, const std::vector<std::string> & patterns) {
    for (const auto & pattern : patterns) {
        if (cmd.find(pattern) == 0) {
            return true;
        }
    }
    return false;
}

bool permission_policy::contains_pattern(const std::string & cmd, const std::vector<std::string> & patterns) {
    for (const auto & pattern : patterns) {
        if (cmd.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool permission_policy::is_path_in_project(const std::string & path) const {
    if (project_root_.empty()) {
        return true;
    }

    try {
        const std::string abs_path = fs::absolute(path).string();
        if (abs_path == project_root_) {
            return true;
        }

        std::string prefix = project_root_;
        if (prefix.back() != '/' && prefix.back() != '\\') {
            prefix += '/';
        }
        return abs_path.find(prefix) == 0;
    } catch (...) {
        return false;
    }
}