// andy-agent - HTTP-only mode (no local inference, no llama.cpp/ggml deps)

#include "common.h"
#include "arg-parser.h"
#include "base64.hpp"
#include "console.h"
#include "agent-loop.h"
#include "agent-resources.h"
#include "clipboard-image.h"
#include "command-dispatcher.h"
#include "command-executor.h"
#include "config-dir.h"
#include "http-inference-backend.h"
#include "http.h"
#include "multimodal-input.h"
#include "pdf-renderer.h"
#include "terminal-image.h"
#include "tool-registry.h"
#include "permission.h"
#include "log.h"

#ifndef _WIN32
#include "mcp/mcp-server-manager.h"
#include "mcp/mcp-tool-wrapper.h"
#endif

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

// ANSI color macros (mirrored from console.cpp for use in this file)
#ifndef ANSI_COLOR_RED
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GRAY    "\x1b[90m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_BOLD          "\x1b[1m"
#endif

const char * ANDY_AGENT_LOGO = R"(
 █████╗ ███╗   ██╗██████╗ ██╗   ██╗      █████╗  ██████╗ ███████╗███╗   ██╗████████╗
██╔══██╗████╗  ██║██╔══██╗╚██╗ ██╔╝     ██╔══██╗██╔════╝ ██╔════╝████╗  ██║╚══██╔══╝
███████║██╔██╗ ██║██║  ██║ ╚████╔╝█████╗███████║██║  ███╗█████╗  ██╔██╗ ██║   ██║   
██╔══██║██║╚██╗██║██║  ██║  ╚██╔╝ ╚════╝██╔══██║██║   ██║██╔══╝  ██║╚██╗██║   ██║   
██║  ██║██║ ╚████║██████╔╝   ██║        ██║  ██║╚██████╔╝███████╗██║ ╚████║   ██║   
╚═╝  ╚═╝╚═╝  ╚═══╝╚═════╝    ╚═╝        ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝  ╚═══╝   ╚═╝   
                                                                          
)";

#include <signal.h>
#include <filesystem>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#   define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Global interrupt flag
// ---------------------------------------------------------------------------

static std::atomic<bool> g_is_interrupted(false);

static void signal_handler(int signum) {
    g_is_interrupted.store(true);
}

// ---------------------------------------------------------------------------
// Stdin helpers
// ---------------------------------------------------------------------------

static bool is_stdin_terminal() {
#if defined(_WIN32)
    return _isatty(_fileno(stdin));
#else
    return isatty(fileno(stdin));
#endif
}

static std::string read_stdin_prompt() {
    std::string result;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!result.empty()) result += "\n";
        result += line;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Help text
// ---------------------------------------------------------------------------

static const char* get_help_text() {
    return
        "andy-agent (HTTP-only mode)\n\n"
        "Usage: andy-agent [OPTIONS]\n\n"
        "Options:\n"
        "  -u, --url URL          Server URL (e.g., http://localhost:8080)\n"
        "  -m, --model MODEL      Model name\n"
        "  -p, --prompt PROMPT    Initial prompt\n"
        "  --yolo                 YOLO mode (auto-approve all permissions)\n"
        "  --max-iterations N     Maximum agent iterations (0 = unlimited)\n"
        "  --session PATH         Session file path\n"
        "  --resume               Resume latest session\n"
        "  --no-session           Disable session persistence\n"
        "  --no-mcp               Disable MCP tools\n"
        "  --no-skills            Disable skills\n"
        "  --no-agents-md         Disable AGENTS.md discovery\n"
        "  --no-compaction        Disable context compaction\n"
        "  --skills-path PATH     Extra skills search path\n"
        "  --simple-io            Simple I/O mode\n"
        "  --no-color             Disable colored output\n"
        "  -v, --verbose          Verbose logging\n"
        "  -h, --help             Show this help\n\n";
}

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------

struct agent_config_params {
    std::string url;
    std::string model;
    std::string prompt;
    bool single_turn = false;
    int verbosity = LOG_LEVEL_ERROR;
    bool simple_io = false;
    bool use_color = true;
    bool yolo_mode = false;
    int max_iterations = 0;
    bool enable_mcp = true;
    bool enable_skills = true;
    bool enable_agents_md = true;
    bool enable_compaction = true;
    bool enable_session = true;
    bool resume_session = false;
    bool show_banner = true;
    std::string session_path;
    std::string working_dir;
    std::vector<std::string> extra_skills_paths;
    bool multiline_input = false;
};

static bool parse_agent_args(int argc, char ** argv, agent_config_params & params) {
    arg_parser parser(argc, argv);

    // Boolean flags
    parser.handle("--yolo", [&](auto) {
        params.yolo_mode = true;
        return true;
    });
    parser.handle("--no-mcp", [&](auto) {
        params.enable_mcp = false;
        return true;
    });
    parser.handle("--no-skills", [&](auto) {
        params.enable_skills = false;
        return true;
    });
    parser.handle("--no-agents-md", [&](auto) {
        params.enable_agents_md = false;
        return true;
    });
    parser.handle("--no-compaction", [&](auto) {
        params.enable_compaction = false;
        return true;
    });
    parser.handle("--resume", [&](auto) {
        params.resume_session = true;
        return true;
    });
    parser.handle("--no-session", [&](auto) {
        params.enable_session = false;
        return true;
    });
    parser.handle("--simple-io", [&](auto) {
        params.simple_io = true;
        return true;
    });
    parser.handle("--no-color", [&](auto) {
        params.use_color = false;
        return true;
    });
    parser.handle("--no-banner", [&](auto) {
        params.show_banner = false;
        return true;
    });
    parser.handle("-v", [&](auto) {
        params.verbosity = LOG_LEVEL_INFO;
        return true;
    });
    parser.handle("--verbose", [&](auto) {
        params.verbosity = LOG_LEVEL_INFO;
        return true;
    });

    // Flags with values
    parser.handle("--url", [&](const std::string& value) {
        params.url = value;
        return true;
    });
    parser.handle("-u", [&](const std::string& value) {
        params.url = value;
        return true;
    });
    parser.handle("--model", [&](const std::string& value) {
        params.model = value;
        return true;
    });
    parser.handle("-m", [&](const std::string& value) {
        params.model = value;
        return true;
    });
    parser.handle("--prompt", [&](const std::string& value) {
        params.prompt = value;
        return true;
    });
    parser.handle("-p", [&](const std::string& value) {
        params.prompt = value;
        return true;
    });
    parser.handle("--session", [&](const std::string& value) {
        params.session_path = value;
        return true;
    });
    parser.handle("--skills-path", [&](const std::string& value) {
        params.extra_skills_paths.push_back(value);
        return true;
    });
    parser.handle("--cwd", [&](const std::string& value) {
        params.working_dir = value;
        return true;
    });
    parser.handle("--working-dir", [&](const std::string& value) {
        params.working_dir = value;
        return true;
    });
    parser.handle("--max-iterations", [&](const std::string& value) {
        try {
            params.max_iterations = std::stoi(value);
            if (params.max_iterations < 0) params.max_iterations = 0;
        } catch (...) {
            fprintf(stderr, "Invalid --max-iterations value: %s\n", value.c_str());
            return false;
        }
        return true;
    });
    parser.handle("-mi", [&](const std::string& value) {
        try {
            params.max_iterations = std::stoi(value);
            if (params.max_iterations < 0) params.max_iterations = 0;
        } catch (...) {
            fprintf(stderr, "Invalid --max-iterations value: %s\n", value.c_str());
            return false;
        }
        return true;
    });

    // Help
    parser.handle("--help", [&](auto) {
        fprintf(stdout, "%s", get_help_text());
        return false;
    });
    parser.handle("-h", [&](auto) {
        fprintf(stdout, "%s", get_help_text());
        return false;
    });

    if (!parser.parse()) {
        std::string err = parser.error();
        if (!err.empty()) {
            fprintf(stderr, "Error: %s\n", err.c_str());
        }
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Session setup
// ---------------------------------------------------------------------------

static bool setup_session(agent_config_params& params,
                          const std::string& working_dir,
                          session_file& sf,
                          loaded_session& loaded,
                          const loaded_session*& resume_ptr) {
    resume_ptr = nullptr;

    if (params.enable_session && params.session_path.empty()) {
        std::string config_dir = get_config_dir();
        if (!config_dir.empty()) {
            std::string session_dir = session_file::get_session_dir(config_dir, working_dir);
            if (params.resume_session) {
                params.session_path = session_file::find_latest_session(session_dir);
                if (params.session_path.empty()) {
                    console::log("No previous session found, starting new.\n");
                    params.session_path = session_file::new_session_path(session_dir);
                }
            } else {
                params.session_path = session_file::new_session_path(session_dir);
            }
        }
    }

    if (!params.session_path.empty()) {
        if (params.resume_session || std::filesystem::exists(params.session_path)) {
            auto maybe = session_file::load(params.session_path);
            if (maybe) {
                loaded = std::move(*maybe);
                resume_ptr = &loaded;
            }
        }
        if (sf.open(params.session_path)) {
            if (resume_ptr) {
                sf.set_message_count(resume_ptr->total_messages_in_file);
            }
        }
    }

    return !params.session_path.empty();
}

// ---------------------------------------------------------------------------
// Banner display
// ---------------------------------------------------------------------------

static void display_banner(const inference_backend_meta& inf,
                           const agent_config_params& params,
                           int mcp_tools_count,
                           const agent_resource_discovery& resources,
                           const std::string& session_path,
                           const loaded_session* resume_ptr) {
    // Logo in cyan
    console::set_display(DISPLAY_TYPE_TOOL_STREAM);
    console::log("\n%s\n", ANDY_AGENT_LOGO);
    console::set_display(DISPLAY_TYPE_RESET);

    // Separator line
    console::log("%s", ANSI_BOLD ANSI_COLOR_CYAN "───────────────────────────────────────────────────────" ANSI_COLOR_RESET "\n");

    // Model info in bold
    console::set_display(DISPLAY_TYPE_INFO);
    console::log("  Model    : %s\n", inf.model_name.c_str());
    console::log("  Backend  : HTTP\n");
    console::set_display(DISPLAY_TYPE_RESET);

    // Configuration details
    console::log("  Server   : %s\n", params.url.c_str());
    console::log("  Working  : %s\n", fs::current_path().string().c_str());

    if (params.yolo_mode) {
        console::set_display(DISPLAY_TYPE_ERROR);
        console::log("  Mode     : YOLO (all permissions auto-approved)\n");
        console::set_display(DISPLAY_TYPE_RESET);
    }

    // Resource counts
    if (mcp_tools_count > 0) {
        console::log("  MCP tools: %d\n", mcp_tools_count);
    }
    if (resources.skills_count > 0) {
        console::log("  Skills   : %d\n", resources.skills_count);
    }
    if (resources.agents_md_count > 0) {
        console::log("  AGENTS.md: %d file(s)\n", resources.agents_md_count);
    }
    if (!session_path.empty()) {
        console::log("  Session  : %s%s\n", session_path.c_str(),
                      resume_ptr ? " (resumed)" : " (new)");
    }

    // Separator
    console::log("%s\n", ANSI_BOLD ANSI_COLOR_CYAN "───────────────────────────────────────────────────────" ANSI_COLOR_RESET "\n");
}

// ---------------------------------------------------------------------------
// Resumed session display
// ---------------------------------------------------------------------------

static void display_resumed_session(const loaded_session& loaded) {
    auto extract_text = [](const json& msg) -> std::string {
        if (!msg.contains("content")) return "";
        const auto& c = msg["content"];
        if (c.is_string()) return c.get<std::string>();
        if (c.is_array()) {
            std::string text;
            for (const auto& block : c) {
                if (block.contains("type") && block["type"] == "image_url") {
                    text += "[image]";
                } else if (block.contains("text") && block["text"].is_string()) {
                    text += block["text"].get<std::string>();
                }
            }
            return text;
        }
        return "";
    };

    console::log("\n");
    console::set_display(DISPLAY_TYPE_INFO);
    console::log("─── Resumed Session (%zu messages) ───\n", loaded.messages.size());
    console::set_display(DISPLAY_TYPE_RESET);

    for (size_t i = 0; i < loaded.messages.size(); i++) {
        const auto& m = loaded.messages[i];
        std::string role = m.value("role", "");
        std::string text = extract_text(m);

        // Truncate long messages to keep display compact
        const int max_display = 300;
        if (text.length() > (size_t)max_display) {
            text = text.substr(0, max_display) + "...";
        }

        if (role == "user") {
            console::set_display(DISPLAY_TYPE_USER_INPUT);
            console::log("  [You]   %s\n", text.c_str());
            console::set_display(DISPLAY_TYPE_RESET);
        } else if (role == "assistant") {
            console::set_display(DISPLAY_TYPE_INFO);
            if (!text.empty()) {
                console::log("  [Agent] %s\n", text.c_str());
            }
            if (m.contains("tool_calls") && m["tool_calls"].is_array()) {
                for (const auto& tc : m["tool_calls"]) {
                    if (tc.contains("function")) {
                        std::string name = tc["function"].value("name", "");
                        console::log("    → %s\n", name.c_str());
                    }
                }
            }
            console::set_display(DISPLAY_TYPE_RESET);
        } else if (role == "tool") {
            console::set_display(DISPLAY_TYPE_TOOL_STREAM);
            if (text.length() > 200) {
                text = text.substr(0, 200) + "...";
            }
            console::log("  [Tool]  %s\n", text.c_str());
            console::set_display(DISPLAY_TYPE_RESET);
        }
    }
    console::set_display(DISPLAY_TYPE_INFO);
    console::log("─── end of resumed session ───\n\n");
    console::set_display(DISPLAY_TYPE_RESET);
}

// ---------------------------------------------------------------------------
// Help text display
// ---------------------------------------------------------------------------

static void display_help() {
    console::log("\n");
    console::set_display(DISPLAY_TYPE_INFO);
    console::log("╔═══════════════════════ Andy-Agent Commands ═══════════════════════╗\n");
    console::set_display(DISPLAY_TYPE_RESET);

    console::log("  %s\n", ANSI_BOLD "Session" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "/exit" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "exit the agent" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "/clear" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "clear conversation history" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "/compact" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "manually compact conversation context" ANSI_COLOR_RESET);

    console::log("  %s\n", ANSI_BOLD "Information" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "/stats" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "show token usage statistics" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "/tools" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "list available tools" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "/skills" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "list available skills" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "/agents" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "list discovered AGENTS.md files" ANSI_COLOR_RESET);

    console::log("  %s\n", ANSI_BOLD "Shell" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "!<cmd>" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "run shell command (output shared with LLM)" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "!!<cmd>" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "run shell command (output hidden from LLM)" ANSI_COLOR_RESET);

    console::log("  %s\n", ANSI_BOLD "Input" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "Ctrl+V" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "paste image from clipboard" ANSI_COLOR_RESET);
    console::log("    %s  %s\n", ANSI_COLOR_GREEN "ESC / Ctrl+C" ANSI_COLOR_RESET, ANSI_COLOR_GRAY "abort generation" ANSI_COLOR_RESET);

    console::set_display(DISPLAY_TYPE_INFO);
    console::log("╚═══════════════════════════════════════════════════════════════════╝\n");
    console::set_display(DISPLAY_TYPE_RESET);
}

// ---------------------------------------------------------------------------
// PDF auto-detection and conversion
// ---------------------------------------------------------------------------

static std::vector<image_entry> extract_pdf_images(const std::string& text,
                                                    const std::string& working_dir) {
    std::vector<image_entry> pdf_images;
    
    // Simple PDF path detection: look for .pdf extensions in the text
    // This handles paths like "/path/to/file.pdf" or "./file.pdf"
    std::string search_text = text;
    size_t pos = 0;
    
    while ((pos = search_text.find(".pdf", pos)) != std::string::npos) {
        // Require .pdf at word boundary to avoid false positives
        // like "generiere ein .pdf" or "conversion.pdf.js"
        size_t ext_end = pos + 4;
        if (ext_end < search_text.size() && 
            (std::isalnum(static_cast<unsigned char>(search_text[ext_end])) || 
             search_text[ext_end] == '.')) {
            // Extension continues (e.g., .pdf.js, .pdfinfo) — skip
            pos = ext_end;
            continue;
        }

        // Extract potential file path around the .pdf extension
        size_t path_start = pos;
        size_t path_end = ext_end;
        
        // Look backwards for path separators, quotes, or start of string
        while (path_start > 0 && search_text[path_start - 1] != ' ' && 
               search_text[path_start - 1] != '\n' && search_text[path_start - 1] != '\t' &&
               search_text[path_start - 1] != '"' && search_text[path_start - 1] != '\'') {
            path_start--;
        }
        
        // Look forwards for end of path
        while (path_end < search_text.size() && search_text[path_end] != ' ' &&
               search_text[path_end] != '\n' && search_text[path_end] != '\t' &&
               search_text[path_end] != '"' && search_text[path_end] != '\'') {
            path_end++;
        }
        
        std::string potential_path = search_text.substr(path_start, path_end - path_start);
        
        // Try to resolve the path
        std::string full_path;
        if (potential_path[0] == '/') {
            full_path = potential_path;
        } else {
            full_path = working_dir + "/" + potential_path;
        }
        
        // Check if file exists and is a PDF
        if (std::filesystem::exists(full_path) && pdf_renderer::is_pdf(full_path)) {
            auto images = multimodal_input::convert_pdf_to_images(full_path);
            if (!images.empty()) {
                pdf_images.insert(pdf_images.end(), 
                                std::make_move_iterator(images.begin()),
                                std::make_move_iterator(images.end()));
            }
        }
        
        pos = path_end;
    }
    
    return pdf_images;
}

// ---------------------------------------------------------------------------
// Shell command execution
// ---------------------------------------------------------------------------

static bool execute_shell_command(const std::string& buffer,
                                  const std::string& working_dir,
                                  agent_loop& agent) {
    bool exclude_from_context = (buffer.size() >= 2 && buffer[1] == '!');
    size_t cmd_start = exclude_from_context ? 2 : 1;
    std::string cmd = buffer.substr(cmd_start);

    // Trim leading whitespace
    size_t first = cmd.find_first_not_of(" \t");
    if (first == std::string::npos) {
        console::log("Usage: !<command> or !!<command>\n");
        return true;
    }
    cmd = cmd.substr(first);

    console::set_display(DISPLAY_TYPE_PROMPT);
    console::log("\n  $ %s\n", cmd.c_str());
    console::set_display(DISPLAY_TYPE_RESET);
    g_is_interrupted.store(false);

    auto cmd_result = command_executor::execute(cmd, working_dir, g_is_interrupted);

    // Ensure output ends with newline for clean display
    if (!cmd_result.output.empty() && cmd_result.output.back() != '\n') {
        fwrite("\n", 1, 1, stdout);
    }

    if (cmd_result.exit_code != 0) {
        console::set_display(DISPLAY_TYPE_ERROR);
        console::log("  ✗ [exit code: %d]\n", cmd_result.exit_code);
        console::set_display(DISPLAY_TYPE_RESET);
    } else {
        console::log("  ✓ [completed]\n");
    }

    if (g_is_interrupted.load()) {
        console::log("  ⏹ [interrupted]\n");
        g_is_interrupted.store(false);
    }

    // Inject into LLM context (single ! only)
    if (!exclude_from_context) {
        std::string context = "[user executed shell command]\n$ " + cmd + "\n" + cmd_result.output;
        if (cmd_result.exit_code != 0) {
            context += "[exit code: " + std::to_string(cmd_result.exit_code) + "]\n";
        }
        agent.add_context_message("user", context);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Command dispatcher setup
// ---------------------------------------------------------------------------

static command_dispatcher setup_command_dispatcher(agent_loop& agent,
                                                    const agent_resource_discovery& resources) {
    command_dispatcher dispatcher;
    command_context ctx{agent, resources, tool_registry::instance()};

    dispatcher.register_command("exit", [](auto, auto) {
        return command_action::EXIT;
    });
    dispatcher.register_command("quit", [](auto, auto) {
        return command_action::EXIT;
    });
    dispatcher.register_command("clear", [](auto, const command_context& c) {
        c.agent.clear();
        console::log("Conversation cleared.\n");
        return command_action::CONTINUE;
    });
    dispatcher.register_command("compact", [](auto, const command_context& c) {
        console::log("\nCompacting...\n");
        if (c.agent.compact()) {
            console::log("Context compacted.\n");
        } else {
            console::log("Nothing to compact (conversation too short).\n");
        }
        return command_action::CONTINUE;
    });
    dispatcher.register_command("tools", [](auto, const command_context& c) {
        console::log("\nAvailable tools:\n");
        for (const auto* tool : c.tools.get_all_tools()) {
            console::log("  %s:\n", tool->name.c_str());
            console::log("    %s\n", tool->description.c_str());
        }
        return command_action::CONTINUE;
    });
    dispatcher.register_command("stats", [](auto, const command_context& c) {
        const auto& stats = c.agent.get_stats();
        console::log("\nSession Statistics:\n");
        console::log("  Prompt tokens:  %d\n", stats.total_input);
        console::log("  Output tokens:  %d\n", stats.total_output);
        if (stats.total_cached > 0) {
            console::log("  Cached tokens:  %d\n", stats.total_cached);
        }
        console::log("  Total tokens:   %d\n", stats.total_input + stats.total_output);
        if (stats.total_prompt_ms > 0) {
            console::log("  Prompt time:    %.2fs\n", stats.total_prompt_ms / 1000.0);
        }
        if (stats.total_predicted_ms > 0) {
            console::log("  Gen time:       %.2fs\n", stats.total_predicted_ms / 1000.0);
            double avg_speed = stats.total_output * 1000.0 / stats.total_predicted_ms;
            console::log("  Avg speed:      %.1f tok/s\n", avg_speed);
        }
        return command_action::CONTINUE;
    });
    dispatcher.register_command("skills", [](auto, const command_context& c) {
        const auto& skills = c.resources.skills.get_skills();
        if (skills.empty()) {
            console::log("\nNo skills discovered.\n");
            console::log("Skills are loaded from:\n");
            console::log("  ./.andy-agent/skills/  (project-local)\n");
            console::log("  ~/.andy-agent/skills/  (user-global)\n");
        } else {
            console::log("\nAvailable skills:\n");
            for (const auto& skill : skills) {
                console::log("  %s:\n", skill.name.c_str());
                console::log("    %s\n", skill.description.c_str());
                console::log("    Path: %s\n", skill.path.c_str());
            }
        }
        return command_action::CONTINUE;
    });
    dispatcher.register_command("agents", [](auto, const command_context& c) {
        const auto& files = c.resources.agents_md.get_files();
        if (files.empty()) {
            console::log("\nNo AGENTS.md files discovered.\n");
            console::log("AGENTS.md files are searched from:\n");
            console::log("  ./AGENTS.md to git root  (project-specific)\n");
            console::log("  ~/.andy-agent/AGENTS.md  (global)\n");
        } else {
            console::log("\nDiscovered AGENTS.md files (closest first):\n");
            for (const auto& file : files) {
                console::log("  %s", file.relative_path.c_str());
                if (file.depth == 0) {
                    console::log(" (highest precedence)");
                }
                console::log("\n    %zu bytes\n", file.content.size());
            }
        }
        return command_action::CONTINUE;
    });

    return dispatcher;
}

// ---------------------------------------------------------------------------
// Status bar update
// ---------------------------------------------------------------------------

static void update_status_bar(const std::string& model_name,
                              const session_stats& stats,
                              int iteration = 0) {
    console::status_bar([&](std::string& out) {
        out = "   " + model_name;
        int total_tokens = stats.total_input + stats.total_output;
        out += " | " + std::to_string(total_tokens) + " tok";
        if (iteration > 0) {
            out += " | #" + std::to_string(iteration);
        }
        if (stats.total_cached > 0) {
            out += " | " + std::to_string(stats.total_cached) + " cache";
        }
        if (stats.total_predicted_ms > 0) {
            double speed = stats.total_output * 1000.0 / stats.total_predicted_ms;
            out += " | " + std::to_string((int)speed) + " tok/s";
        }
    });
}

// ---------------------------------------------------------------------------
// Result display
// ---------------------------------------------------------------------------

static void display_result(agent_loop_result& result) {
    console::log("\n");
    switch (result.stop_reason) {
        case agent_stop_reason::COMPLETED: {
            console::set_display(DISPLAY_TYPE_INFO);
            console::log("  ✓ Completed in %d iteration(s)\n", result.iterations);
            console::set_display(DISPLAY_TYPE_RESET);
            break;
        }
        case agent_stop_reason::MAX_ITERATIONS:
            console::set_display(DISPLAY_TYPE_ERROR);
            console::log("  ⚠ Max iterations reached (%d)\n", result.iterations);
            console::set_display(DISPLAY_TYPE_RESET);
            break;
        case agent_stop_reason::USER_CANCELLED:
            console::log("  ⏹ Cancelled by user\n");
            g_is_interrupted.store(false);
            break;
        case agent_stop_reason::AGENT_ERROR:
            console::error("  ✗ Error occurred\n");
            break;
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char ** argv) {
    agent_config_params params;

    // Parse agent-specific flags
    if (!parse_agent_args(argc, argv, params)) {
        return 1;
    }

    // Validate required arguments
    if (params.url.empty()) {
        fprintf(stderr, "Error: --url is required in HTTP-only mode\n");
        fprintf(stderr, "Usage: andy-agent --url http://localhost:8080 [--model MODEL] [OPTIONS]\n");
        return 1;
    }

    // Initialize
    common_init();
    common_log_set_verbosity_thold(params.verbosity);
    console::init(params.simple_io, params.use_color);
    atexit([]() { console::cleanup(); });

    // Register clipboard image paste handler for Ctrl+V
    console::set_paste_image_callback([](std::vector<uint8_t>& bytes, std::string& mime) -> bool {
        auto img = clipboard_read_image();
        if (!img) return false;
        bytes = std::move(img->bytes);
        mime  = std::move(img->mime_type);
        return true;
    });

    console::set_display(DISPLAY_TYPE_RESET);

    // Signal handlers
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
    struct sigaction sigint_action;
    sigint_action.sa_handler = signal_handler;
    sigemptyset(&sigint_action.sa_mask);
    sigint_action.sa_flags = 0;
    sigaction(SIGINT, &sigint_action, nullptr);
    sigaction(SIGTERM, &sigint_action, nullptr);
#elif defined (_WIN32)
    auto console_ctrl_handler = +[](DWORD ctrl_type) -> BOOL {
        return (ctrl_type == CTRL_C_EVENT) ? (signal_handler(SIGINT), true) : false;
    };
    SetConsoleCtrlHandler(reinterpret_cast<PHANDLER_ROUTINE>(console_ctrl_handler), true);
#endif

    // Create HTTP backend
    http_inference_backend_config http_cfg;
    http_cfg.base_url = params.url;
    if (!params.model.empty()) {
        http_cfg.model = params.model;
    }

    auto http_backend = std::make_unique<http_inference_backend>(std::move(http_cfg));
    inference_backend* inference = http_backend.get();
    inference_backend_meta inf = inference->meta();

    // Get working directory (--cwd/--working-dir overrides current path)
    std::string working_dir = params.working_dir.empty()
        ? fs::current_path().string()
        : params.working_dir;

    // Load MCP servers (Unix only)
#ifndef _WIN32
    mcp_server_manager mcp_mgr;
    int mcp_tools_count = 0;
    if (params.enable_mcp) {
        std::string mcp_config = find_mcp_config(working_dir);
        if (!mcp_config.empty()) {
            if (mcp_mgr.load_config(mcp_config)) {
                int started = mcp_mgr.start_servers();
                if (started > 0) {
                    register_mcp_tools(mcp_mgr);
                    mcp_tools_count = (int)mcp_mgr.list_all_tools().size();
                }
            }
        }
    }
#else
    int mcp_tools_count = 0;
#endif

    // Discover resources
    agent_resource_config resource_cfg;
    resource_cfg.working_dir = working_dir;
    resource_cfg.config_dir = get_config_dir();
    resource_cfg.enable_skills = params.enable_skills;
    resource_cfg.enable_agents_md = params.enable_agents_md;
    resource_cfg.extra_skills_paths = params.extra_skills_paths;
    agent_resource_discovery resources = agent_discover_resources(resource_cfg);

    if (params.enable_agents_md && resources.agents_md_total_content_size > 50 * 1024) {
        console::log("Warning: AGENTS.md content is large (%zu bytes). "
                    "Consider reducing size for better performance.\n",
                    resources.agents_md_total_content_size);
    }

    // Configure agent
    agent_config config;
    config.working_dir = working_dir;
    config.max_iterations = params.max_iterations;
    config.tool_timeout_ms = 120000;
    config.verbose = (params.verbosity >= LOG_LEVEL_INFO);
    config.yolo_mode = params.yolo_mode;
    config.enable_skills = params.enable_skills;
    config.skills_search_paths = params.extra_skills_paths;
    config.skills_prompt_section = resources.skills_prompt_section();
    config.enable_agents_md = params.enable_agents_md;
    config.agents_md_prompt_section = resources.agents_md_prompt_section();
    config.compaction.enabled = params.enable_compaction;
    if (inf.is_llama_server && inf.total_slots == 1) {
        config.inference_id_slot = 0;
    }

    // Session persistence
    session_file sf;
    session_file* sf_ptr = nullptr;
    loaded_session loaded;
    const loaded_session* resume_ptr = nullptr;

    if (setup_session(params, working_dir, sf, loaded, resume_ptr)) {
        sf_ptr = &sf;
    }

    // Create permission manager for streaming (async, non-blocking)
    permission_manager_async async_perms;
    async_perms.set_project_root(working_dir);
    async_perms.set_yolo_mode(params.yolo_mode);

    // Create agent loop
    agent_loop agent(*inference, config, g_is_interrupted, sf_ptr, resume_ptr);

    // Display banner
    display_banner(inf, params, mcp_tools_count, resources, params.session_path, resume_ptr);

    // Display resumed conversation history
    if (resume_ptr && !resume_ptr->messages.empty()) {
        display_resumed_session(*resume_ptr);
    }

    // Resolve initial prompt from -p/--prompt flag or stdin
    std::string initial_prompt;
    if (!params.prompt.empty()) {
        initial_prompt = params.prompt;
    } else if (!is_stdin_terminal()) {
        initial_prompt = read_stdin_prompt();
        while (!initial_prompt.empty() && (initial_prompt.back() == '\n' || initial_prompt.back() == '\r')) {
            initial_prompt.pop_back();
        }
        params.single_turn = true;
    }

    // Non-interactive mode: skip help text
    if (initial_prompt.empty() || !params.single_turn) {
        display_help();
    }

    // Setup command dispatcher
    command_dispatcher dispatcher = setup_command_dispatcher(agent, resources);

    // Track if we have an initial prompt to process
    bool first_turn = !initial_prompt.empty();

    // -----------------------------------------------------------------------
    // Live streaming callback: prints text/reasoning/tool-state as it arrives
    // -----------------------------------------------------------------------
    auto make_streaming_callback = [&]() {
        return [&](const agent_event & event) {
            switch (event.type) {
                case agent_event_type::TEXT_DELTA: {
                    console::log("%s", event.data["content"].get<std::string>().c_str());
                    console::flush();
                    break;
                }
                case agent_event_type::REASONING_DELTA: {
                    console::set_display(DISPLAY_TYPE_REASONING);
                    console::log("%s", event.data["content"].get<std::string>().c_str());
                    console::flush();
                    console::set_display(DISPLAY_TYPE_RESET);
                    break;
                }
                case agent_event_type::TOOL_START: {
                    console::log("\n");
                    console::set_display(DISPLAY_TYPE_TOOL_STREAM);
                    console::log("  ⚙ %s", event.data["name"].get<std::string>().c_str());
                    console::set_display(DISPLAY_TYPE_RESET);
                    console::log(" ... ");
                    console::flush();
                    break;
                }
                case agent_event_type::TOOL_RESULT: {
                    bool success = event.data["success"];
                    if (success) {
                        console::log("✓");
                    } else {
                        console::set_display(DISPLAY_TYPE_ERROR);
                        console::log("✗");
                        console::set_display(DISPLAY_TYPE_RESET);
                    }
                    console::log("\n");
                    console::flush();
                    break;
                }
                case agent_event_type::COMPACTION_COMPLETED: {
                    int32_t kept = event.data["messages_kept"];
                    console::set_display(DISPLAY_TYPE_TOOL_STREAM);
                    console::log("\n  🗜 Context compacted (%d messages kept)\n", kept);
                    console::set_display(DISPLAY_TYPE_RESET);
                    console::flush();
                    break;
                }
                case agent_event_type::ERROR: {
                    console::set_display(DISPLAY_TYPE_ERROR);
                    console::log("\n  ✗ %s\n", event.data["message"].get<std::string>().c_str());
                    console::set_display(DISPLAY_TYPE_RESET);
                    console::flush();
                    break;
                }
                case agent_event_type::PERMISSION_REQUIRED: {
                    // Handle permission prompt inline using readline (not scanf,
                    // which conflicts with the streaming stdin buffer).
                    std::string req_id  = event.data["request_id"];
                    std::string tool    = event.data["tool"];
                    std::string details = event.data["details"];
                    bool dangerous      = event.data.value("dangerous", false);

                    console::log("\n");
                    if (dangerous) {
                        console::set_display(DISPLAY_TYPE_ERROR);
                    } else {
                        console::set_display(DISPLAY_TYPE_PROMPT);
                    }
                    console::log("  ⚡ Permission required: %s\n", tool.c_str());
                    console::log("     %s\n", details.c_str());
                    console::log("  [y]es [n]o [a]lways [d]eny: ");
                    console::set_display(DISPLAY_TYPE_RESET);
                    console::flush();

                    // Use readline to get a clean line of input (avoids scanf
                    // reading stale bytes from the streaming stdin buffer).
                    std::string response_line;
                    console::readline(response_line, false);
                    if (!response_line.empty() && response_line.back() == '\n') {
                        response_line.pop_back();
                    }

                    char response_char = response_line.empty() ? '\0' : response_line[0];
                    bool allowed = (response_char == 'y' || response_char == 'Y' ||
                                    response_char == 'a' || response_char == 'A');
                    permission_scope scope = permission_scope::ONCE;
                    if (response_char == 'a' || response_char == 'A') {
                        scope = permission_scope::SESSION;
                    } else if (response_char == 'd' || response_char == 'D') {
                        scope = permission_scope::SESSION;
                        allowed = false;
                    } else if (response_char != 'y' && response_char != 'Y') {
                        allowed = false;
                    }
                    async_perms.respond(req_id, allowed, scope);
                    break;
                }
                default:
                    break;
            }
        };
    };

    // Main loop
    while (true) {
        std::string buffer;
        std::vector<image_entry> pasted_images;

        if (first_turn) {
            buffer = initial_prompt;
            first_turn = false;
            console::set_display(DISPLAY_TYPE_USER_INPUT);
            console::log("\n› %s\n", buffer.c_str());
            console::set_display(DISPLAY_TYPE_RESET);
        } else {
            // Interactive input
            console::clear_status_bar();
            console::set_display(DISPLAY_TYPE_USER_INPUT);
            console::log("\n› ");

            std::string line;
            bool another_line = true;
            do {
                another_line = console::readline(line, params.multiline_input);
                buffer += line;
            } while (another_line);

            console::set_display(DISPLAY_TYPE_RESET);
            auto pending = console::take_pending_images();
            pasted_images.reserve(pasted_images.size() + pending.size());
            for (auto& [bytes, mime] : pending) {
                pasted_images.push_back({std::move(bytes), std::move(mime)});
            }

            if (g_is_interrupted.load()) {
                g_is_interrupted.store(false);
                break;
            }

            // Remove trailing newline
            if (!buffer.empty() && buffer.back() == '\n') {
                buffer.pop_back();
            }

            // Skip empty input (unless images were pasted)
            if (buffer.empty() && pasted_images.empty()) {
                continue;
            }

            // Handle ! prefix: run shell command
            if (!buffer.empty() && buffer[0] == '!') {
                execute_shell_command(buffer, working_dir, agent);
                continue;
            }

            // Dispatch slash commands
            if (dispatcher.dispatch(buffer, command_context{agent, resources, tool_registry::instance()}) == command_action::EXIT) {
                break;
            }
            
            // Extract PDF images from text (auto-detect .pdf paths)
            auto pdf_images = extract_pdf_images(buffer, working_dir);
            if (!pdf_images.empty()) {
                console::log("[Detected %zu PDF page(s) as images]\n", pdf_images.size());
                pasted_images.insert(pasted_images.end(),
                                   std::make_move_iterator(pdf_images.begin()),
                                   std::make_move_iterator(pdf_images.end()));
            }
            
            // Unknown command — treat as user message
            json user_content = multimodal_input::build_content(
                buffer, pasted_images, inf.has_vision);
            auto on_event1 = make_streaming_callback();
            agent_loop_result result = agent.run_streaming(user_content, on_event1, nullptr, &async_perms);
            display_result(result);
            update_status_bar(inf.model_name, agent.get_stats(), result.iterations);
            if (params.single_turn) {
                break;
            }
            continue;
        }

        // Extract PDF images from text (auto-detect .pdf paths)
        auto pdf_images = extract_pdf_images(buffer, working_dir);
        if (!pdf_images.empty()) {
            console::log("[Detected %zu PDF page(s) as images]\n", pdf_images.size());
            pasted_images.insert(pasted_images.end(),
                               std::make_move_iterator(pdf_images.begin()),
                               std::make_move_iterator(pdf_images.end()));
        }

        // Build user content — multimodal if images were pasted
        json user_content = multimodal_input::build_content(
            buffer, pasted_images, inf.has_vision);

        // If model lacks vision but images were pasted, show warning
        if (!pasted_images.empty() && !multimodal_input::can_show_images(pasted_images, inf.has_vision)) {
            console::set_display(DISPLAY_TYPE_ERROR);
            console::log("[model lacks vision — %zu image(s) not included]\n",
                         pasted_images.size());
            console::set_display(DISPLAY_TYPE_RESET);
        }

        // Run agent loop with live streaming output
        auto on_event2 = make_streaming_callback();
        agent_loop_result result = agent.run_streaming(user_content, on_event2, nullptr, &async_perms);

        // Display result
        display_result(result);

        // Update status bar
        update_status_bar(inf.model_name, agent.get_stats(), result.iterations);

        if (params.single_turn) {
            break;
        }
    }

    console::set_display(DISPLAY_TYPE_RESET);
    console::log("\nExiting...\n");

#ifndef _WIN32
    // Shutdown MCP servers
    mcp_mgr.shutdown_all();
#endif

    common_log_set_verbosity_thold(LOG_LEVEL_INFO);

    return 0;
}
