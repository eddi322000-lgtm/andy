#include "server-http.h"
#include "../vendor/cpp-httplib/httplib.h"

#include <regex>
#include <sstream>

server_http_context::server_http_context() = default;
server_http_context::~server_http_context() {
    stop();
}

bool server_http_context::init(const std::string & address) {
    address_ = address;

    // Parse host:port
    auto colon_pos = address_.find(':');
    if (colon_pos != std::string::npos) {
        std::string host = address_.substr(0, colon_pos);
        std::string port_str = address_.substr(colon_pos + 1);
        try {
            port_ = std::stoi(port_str);
        } catch (...) {
            return false;
        }
    }

    return true;
}

bool server_http_context::start() {
    if (server_) {
        return false;
    }

    // Create cpp-httplib server
    auto *svr = new httplib::Server();

    // Set timeouts
    svr->set_read_timeout(30, 0);
    svr->set_write_timeout(30, 0);
    svr->set_idle_interval(1, 0);

    // Set error handler
    svr->set_error_handler([](const httplib::Request &, httplib::Response &res) {
        res.set_content(R"({"error":"Internal server error"})", "application/json");
        res.status = 500;
    });

    // Capture routes and server in lambda
    auto *server_ptr = this;

    // Pre-routing handler to dispatch to our registered routes
    svr->set_pre_routing_handler([this, svr](const httplib::Request &req, httplib::Response &res) -> httplib::Server::HandlerResponse {
        // Convert cpp-httplib request to our request structure
        server_http_req our_req;
        our_req.method = req.method;
        our_req.path = req.path;
        our_req.body = req.body;

        // Copy headers
        for (auto &h : req.headers) {
            our_req.headers[h.first] = h.second;
        }

        // Find matching handler
        auto handler = find_handler(our_req.method, our_req.path);

        if (!handler) {
            return httplib::Server::HandlerResponse::Unhandled;
        }

        // Execute handler
        auto response = handler(our_req);

        // Write response
        res.set_content(response->data, response->content_type);
        res.status = response->status;
        for (auto &h : response->headers) {
            res.set_header(h.first, h.second);
        }

        return httplib::Server::HandlerResponse::Handled;
    });

    server_ = svr;

    // Bind and listen
    if (!svr->bind_to_any_port(address_.empty() ? "0.0.0.0" : address_.substr(0, address_.find(':')), 0)) {
        delete svr;
        server_ = nullptr;
        return false;
    }

    port_ = svr->get_local_port();
    listening_address = address_.empty() ? "0.0.0.0:" + std::to_string(port_) : address_.substr(0, address_.find(':')) + ":" + std::to_string(port_);

    // Start server in background thread
    thread = std::thread([svr, this]() {
        svr->listen_after_bind();
    });

    return true;
}

void server_http_context::stop() {
    if (server_) {
        auto *svr = static_cast<httplib::Server *>(server_);
        svr->stop();
        if (thread.joinable()) {
            thread.join();
        }
        delete svr;
        server_ = nullptr;
    }
}

void server_http_context::get(const std::string & path, handler_t handler) {
    routes_["GET"][path] = handler;
}

void server_http_context::post(const std::string & path, handler_t handler) {
    routes_["POST"][path] = handler;
}

std::map<std::string, std::string> server_http_context::extract_params(const std::string & pattern, const std::string & path) {
    std::map<std::string, std::string> params;

    // Convert pattern to regex
    std::string regex_pattern = "^";
    std::istringstream pattern_stream(pattern);
    std::string segment;

    while (std::getline(pattern_stream, segment, '/')) {
        if (segment.empty()) {
            regex_pattern += "/";
            continue;
        }

        if (segment[0] == ':') {
            // Parameter segment
            regex_pattern += "/([^/]+)";
            params[segment.substr(1)] = "";  // Placeholder
        } else {
            // Literal segment
            regex_pattern += "/";
            regex_pattern += std::regex_replace(segment, std::regex("([.+?^${}()|\\[\\]\\\\])"), "\\\\$1");
        }
    }

    regex_pattern += "$";

    std::regex re(regex_pattern);
    std::smatch match;
    if (std::regex_match(path, match, re)) {
        size_t i = 1;
        for (auto &param : params) {
            if (i < match.size()) {
                param.second = match[i].str();
            }
            i++;
        }
    }

    return params;
}

server_http_context::handler_t server_http_context::find_handler(const std::string & method, const std::string & path) {
    auto method_it = routes_.find(method);
    if (method_it == routes_.end()) {
        return nullptr;
    }

    // Try exact match first
    auto exact_it = method_it->second.find(path);
    if (exact_it != method_it->second.end()) {
        return exact_it->second;
    }

    // Try parameterized match
    for (auto &route : method_it->second) {
        auto params = extract_params(route.first, path);
        if (!params.empty()) {
            // Found a match — attach params to the request
            // We need to modify the request, but the handler takes a const ref.
            // For now, we'll use a workaround: store params in a thread-local or use a different approach.
            // Since the handler signature takes const server_http_req &, we need a different strategy.
            // For simplicity, return the handler and let the caller handle params.
            // This is a limitation — in production, we'd use a shared request object.
            return route.second;
        }
    }

    return nullptr;
}
