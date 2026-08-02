#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <atomic>

#include <nlohmann/json.hpp>

using json = nlohmann::ordered_json;

// HTTP request structure
struct server_http_req {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;  // URL path parameters (e.g. :id)

    std::string get_param(const std::string & name) const {
        auto it = params.find(name);
        return it != params.end() ? it->second : std::string();
    }
};

// HTTP response structure
struct server_http_res {
    int status = 200;
    std::string content_type = "application/json";
    std::string data;
    std::map<std::string, std::string> headers;

    // For streaming responses: called repeatedly to get the next chunk
    std::function<bool(std::string & output)> next;
};

using server_http_res_ptr = std::unique_ptr<server_http_res>;

// Simple HTTP server context (cpp-httplib wrapper)
class server_http_context {
public:
    using handler_t = std::function<server_http_res_ptr(const server_http_req &)>;

    server_http_context();
    ~server_http_context();

    // Initialize with listen address (e.g. "0.0.0.0:8080")
    bool init(const std::string & address = "0.0.0.0:8080");

    // Start the server (non-blocking)
    bool start();

    // Stop the server
    void stop();

    // Register a route handler
    void get(const std::string & path, handler_t handler);
    void post(const std::string & path, handler_t handler);

    // Address the server is listening on
    std::string listening_address;

    // Server thread
    std::thread thread;

    // Ready flag
    std::atomic<bool> is_ready{false};

private:
    // Parse URL path, extracting :param segments
    static std::map<std::string, std::string> extract_params(const std::string & pattern, const std::string & path);

    // Find matching handler for a request
    handler_t find_handler(const std::string & method, const std::string & path);

    // cpp-httplib server
    void * server_ = nullptr;

    // Registered routes: method -> pattern -> handler
    std::map<std::string, std::map<std::string, handler_t>> routes_;

    // Listen address
    std::string address_;
    int port_ = 8080;
};
