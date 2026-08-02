#include "command-dispatcher.h"
#include "agent-loop.h"
#include "agent-resources.h"
#include "tool-registry.h"

#include <algorithm>
#include <iostream>

void command_dispatcher::register_command(const std::string& name, command_handler handler) {
    commands_[name] = std::move(handler);
}

command_action command_dispatcher::dispatch(const std::string& input, const command_context& ctx) {
    // Strip leading /
    std::string cmd = input;
    if (!cmd.empty() && cmd[0] == '/') {
        cmd = cmd.substr(1);
    }

    // Trim trailing whitespace
    while (!cmd.empty() && (cmd.back() == ' ' || cmd.back() == '\t')) {
        cmd.pop_back();
    }

    auto it = commands_.find(cmd);
    if (it != commands_.end()) {
        return it->second(cmd, ctx);
    }

    return command_action::CONTINUE;
}

bool command_dispatcher::has_command(const std::string& name) const {
    return commands_.find(name) != commands_.end();
}

std::vector<std::string> command_dispatcher::list_commands() const {
    std::vector<std::string> names;
    names.reserve(commands_.size());
    for (const auto& [name, _] : commands_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}
