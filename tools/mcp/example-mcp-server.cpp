// Example: Run andy-agent as an MCP server
// This demonstrates the new MCP server export mode
//
// Usage:
//   ./example-mcp-server
//
// Then connect with an MCP client:
//   echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test-client","version":"1.0.0"}}}' | nc localhost 3000
//
// Or use the MCP CLI:
//   mcp --server-command "./example-mcp-server"

#include "mcp-server.h"
#include "../tool-registry.h"

#include <iostream>
#include <thread>
#include <signal.h>

static bool running = true;

void signal_handler(int) {
    running = false;
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "andy-agent MCP Server Example" << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Exporting tools from tool registry..." << std::endl;

    // Get all registered tools
    auto all_tools = tool_registry::instance().get_all_tools();
    std::cout << "Found " << all_tools.size() << " tools to export" << std::endl;

    // Convert to MCP tool format
    std::vector<mcp_server_tool> mcp_tools;
    for (const auto & tool : all_tools) {
        mcp_server_tool mcp_tool;
        mcp_tool.name = tool->name;
        mcp_tool.description = tool->description;
        mcp_tool.input_schema = json::parse(tool->parameters);
        mcp_tools.push_back(mcp_tool);
        std::cout << "  - " << tool->name << ": " << tool->description << std::endl;
    }

    // Create tool handler that delegates to tool registry
    std::function<json(const std::string &, const json &)> handler = [](const std::string & tool_name, const json & arguments) -> json {
        auto tool = tool_registry::instance().get_tool(tool_name);
        if (!tool) {
            return {
                {"content", json::array({{
                    {"type", "text"},
                    {"text", "Tool not found: " + tool_name}}
                })},
                {"isError", true}
            };
        }

        // Execute tool
        tool_context ctx;
        tool_result result = tool->execute(arguments, ctx);

        // Convert result to MCP format
        json content = json::array();
        if (!result.output.empty()) {
            content.push_back({
                {"type", "text"},
                {"text", result.output}
            });
        }
        if (!result.error.empty()) {
            content.push_back({
                {"type", "text"},
                {"text", "Error: " + result.error}
            });
        }

        return {
            {"content", content},
            {"isError", !result.success}
        };
    };

    // Start the MCP server (blocks until shutdown)
    std::cout << "\nStarting MCP server on stdio..." << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;

    mcp_server server;
    server.start("andy-agent-example", "1.0.0", mcp_tools, std::move(handler));

    // Wait for shutdown signal
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down..." << std::endl;
    server.shutdown();

    return 0;
}
