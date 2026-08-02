#pragma once

#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

using json = nlohmann::ordered_json;

// MCP tool definition for server export
struct mcp_server_tool {
    std::string name;
    std::string description;
    json input_schema;
};

// MCP server request handler
using tool_handler_t = std::function<json(const std::string & tool_name, const json & arguments)>;

// MCP server for stdio transport
// Implements JSON-RPC 2.0 over stdin/stdout pipes to MCP client process
class mcp_server {
public:
    mcp_server();
    ~mcp_server();

    // Disable copy
    mcp_server(const mcp_server &) = delete;
    mcp_server & operator=(const mcp_server &) = delete;

    // Start the MCP server (blocks until shutdown)
    // server_name: name to report in initialize response
    // server_version: version to report
    // tools: list of tools to export
    // handler: function to call when a tool is invoked
    void start(const std::string & server_name,
               const std::string & server_version,
               const std::vector<mcp_server_tool> & tools,
               tool_handler_t handler);

    // Graceful shutdown
    void shutdown();

    // Check if server is running
    bool is_running() const { return running_.load(); }

    // Get last error message
    std::string last_error() const { return last_error_; }

    // Get server name
    std::string server_name() const { return server_name_; }

private:
    // Process stdin/stdout for JSON-RPC messages
    void process_loop();

    // Read a single JSON-RPC message from stdin
    json read_message();

    // Write a JSON-RPC message to stdout
    bool write_message(const json & msg);

    // Handle initialize request
    json handle_initialize(const json & params);

    // Handle tools/list request
    json handle_tools_list();

    // Handle tools/call request
    json handle_tools_call(const json & params);

    // Handle notifications
    void handle_notification(const std::string & method, const json & params);

    // Server state
    std::string server_name_;
    std::string server_version_;
    std::vector<mcp_server_tool> tools_;
    tool_handler_t handler_;

    std::atomic<bool> running_{false};
    std::string last_error_;

    // Request ID counter
    int request_id_ = 0;

    // Thread for processing
    std::thread process_thread_;
};
