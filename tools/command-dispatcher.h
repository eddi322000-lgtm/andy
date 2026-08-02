#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

// Forward declarations
class agent_loop;
class agent_resource_discovery;
class tool_registry;

// Result of dispatching a command
enum class command_action {
    CONTINUE,   // Continue REPL loop
    EXIT,       // Exit REPL loop
};

// Context passed to command handlers
struct command_context {
    agent_loop& agent;
    const agent_resource_discovery& resources;
    const tool_registry& tools;
};

// Command handler signature: takes the command argument and context
// Returns the action to take
using command_handler = std::function<command_action(const std::string& arg, const command_context& ctx)>;

// Dispatch table for REPL slash commands.
// Commands are registered by name (without leading /).
// Usage:
//   command_dispatcher dispatcher;
//   dispatcher.register_command("exit", [](auto, auto) { return command_action::EXIT; });
//   dispatcher.register_command("clear", [](auto, auto& ctx) { ctx.agent.clear(); return command_action::CONTINUE; });
//   bool handled = dispatcher.dispatch("/exit", ctx);

class command_dispatcher {
public:
    void register_command(const std::string& name, command_handler handler);

    // Dispatch a command (with or without leading /).
    // Returns command_action::CONTINUE if handled and should continue,
    // command_action::EXIT if handled and should exit,
    // command_action::CONTINUE if unknown (treated as user message).
    command_action dispatch(const std::string& input, const command_context& ctx);

    // Check if a command name is registered.
    bool has_command(const std::string& name) const;

    // Get list of all registered command names.
    std::vector<std::string> list_commands() const;

private:
    std::unordered_map<std::string, command_handler> commands_;
};
