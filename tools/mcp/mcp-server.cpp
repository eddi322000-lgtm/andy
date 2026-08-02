#include "mcp-server.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <unistd.h>
#include <errno.h>

mcp_server::mcp_server() = default;
mcp_server::~mcp_server() {
    shutdown();
}

void mcp_server::start(const std::string & server_name,
                       const std::string & server_version,
                       const std::vector<mcp_server_tool> & tools,
                       tool_handler_t handler) {
    if (running_.load()) {
        last_error_ = "Server already running";
        return;
    }

    server_name_ = server_name;
    server_version_ = server_version;
    tools_ = tools;
    handler_ = std::move(handler);
    running_.store(true);

    // Start processing thread
    process_thread_ = std::thread([this]() {
        process_loop();
    });
}

void mcp_server::shutdown() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (process_thread_.joinable()) {
        process_thread_.join();
    }
}

void mcp_server::process_loop() {
    while (running_.load()) {
        json message = read_message();
        if (message.is_null()) {
            if (!running_.load()) {
                break;
            }
            continue;
        }

        // Check if this is a request or notification
        if (message.contains("id")) {
            // Request
            std::string method = message.value("method", "");
            json params = message.contains("params") ? message["params"] : json::object();
            int id = message["id"].get<int>();

            json response;
            response["jsonrpc"] = "2.0";
            response["id"] = id;

            if (method == "initialize") {
                response["result"] = handle_initialize(params);
            } else if (method == "tools/list") {
                response["result"] = handle_tools_list();
            } else if (method == "tools/call") {
                try {
                    response["result"] = handle_tools_call(params);
                } catch (const std::exception & e) {
                    response["error"] = {
                        {"code", -32603},
                        {"message", std::string("Internal error: ") + e.what()}
                    };
                }
            } else {
                // Unknown method
                response["error"] = {
                    {"code", -32601},
                    {"message", "Method not found: " + method}
                };
            }

            write_message(response);
        } else if (message.contains("method")) {
            // Notification
            std::string method = message["method"].get<std::string>();
            json params = message.contains("params") ? message["params"] : json::object();
            handle_notification(method, params);
        }
    }
}

json mcp_server::read_message() {
    std::string line;
    std::getline(std::cin, line);

    if (std::cin.eof() || std::cin.fail()) {
        return json();
    }

    // Remove trailing \r if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    // Skip empty lines
    if (line.empty()) {
        return read_message();
    }

    try {
        return json::parse(line);
    } catch (const json::parse_error & e) {
        last_error_ = std::string("JSON parse error: ") + e.what();
        return json();
    }
}

bool mcp_server::write_message(const json & msg) {
    std::string data = msg.dump() + "\n";

    size_t total = 0;
    while (total < data.size()) {
        ssize_t n = write(STDOUT_FILENO, data.data() + total, data.size() - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            last_error_ = "Write error: " + std::string(strerror(errno));
            return false;
        }
        total += n;
    }

    return true;
}

json mcp_server::handle_initialize(const json & params) {
    // Accept any protocol version and capabilities
    (void)params;

    return {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {
            {"tools", {
                {"listChanged", true}
            }}
        }},
        {"serverInfo", {
            {"name", server_name_},
            {"version", server_version_}
        }}
    };
}

json mcp_server::handle_tools_list() {
    json tools = json::array();

    for (const auto & tool : tools_) {
        tools.push_back({
            {"name", tool.name},
            {"description", tool.description},
            {"inputSchema", tool.input_schema}
        });
    }

    return {{"tools", tools}};
}

json mcp_server::handle_tools_call(const json & params) {
    std::string tool_name = params.value("name", "");
    json arguments = params.contains("arguments") ? params["arguments"] : json::object();

    if (handler_) {
        try {
            json result = handler_(tool_name, arguments);
            json success_content = json::array();
            success_content.push_back({{"type", "text"}, {"text", "Tool executed successfully"}});
            json response = {
                {"content", result.value("content", success_content)},
                {"isError", result.value("isError", false)}
            };
            return response;
        } catch (const std::exception & e) {
            json error_content = json::array();
            error_content.push_back({{"type", "text"}, {"text", std::string("Tool error: ") + e.what()}});
            return {
                {"content", error_content},
                {"isError", true}
            };
        }
    }

    json no_handler_content = json::array();
    no_handler_content.push_back({{"type", "text"}, {"text", "No handler registered"}});
    return {
        {"content", no_handler_content},
        {"isError", true}
    };
}

void mcp_server::handle_notification(const std::string & method, const json & params) {
    // Handle notifications (no response needed)
    // Currently we don't process any notifications, but we accept them
    (void)method;
    (void)params;
}
