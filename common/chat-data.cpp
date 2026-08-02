// Chat data structures implementation (HTTP-only, no llama.cpp/ggml deps)

#include "chat-data.h"
#include "jinja/caps.h"

#include <nlohmann/json.hpp>

#include <stdexcept>

std::string common_chat_msg::render_content(const std::string & delimiter) const {
    if (!content_parts.empty()) {
        std::string result;
        for (size_t i = 0; i < content_parts.size(); i++) {
            if (content_parts[i].type == "text") {
                if (!result.empty()) result += delimiter;
                result += content_parts[i].text;
            }
        }
        return result;
    }
    return content;
}

json common_chat_msg::to_json_oaicompat(bool concat_typed_text) const {
    json j;
    j["role"] = role;

    if (!content_parts.empty()) {
        if (concat_typed_text) {
            // Concatenate all text parts into a single string
            std::string text;
            for (const auto & part : content_parts) {
                if (part.type == "text") {
                    if (!text.empty()) text += "\n";
                    text += part.text;
                }
            }
            j["content"] = text;
        } else {
            json parts = json::array();
            for (const auto & part : content_parts) {
                if (part.type == "text") {
                    parts.push_back({{"type", "text"}, {"text", part.text}});
                } else if (part.type == "media_marker") {
                    parts.push_back({{"type", "media_marker"}, {"text", part.text}});
                }
            }
            j["content"] = parts;
        }
    } else {
        j["content"] = content;
    }

    if (!tool_calls.empty()) {
        json tool_calls_json = json::array();
        for (const auto & tc : tool_calls) {
            tool_calls_json.push_back({
                {"type", "function"},
                {"function", {
                    {"name", tc.name},
                    {"arguments", tc.arguments},
                }},
            });
            if (!tc.id.empty()) {
                tool_calls_json.back()["id"] = tc.id;
            }
        }
        j["tool_calls"] = tool_calls_json;
    }

    if (!reasoning_content.empty()) {
        j["reasoning_content"] = reasoning_content;
    }

    if (!tool_name.empty()) {
        j["name"] = tool_name;
    }

    if (!tool_call_id.empty()) {
        j["tool_call_id"] = tool_call_id;
    }

    return j;
}

std::vector<common_chat_msg> common_chat_msgs_parse_oaicompat(const json & messages) {
    std::vector<common_chat_msg> msgs;

    try {
        if (!messages.is_array()) {
            throw std::invalid_argument("Expected 'messages' to be an array, got " + messages.dump());
        }

        for (const auto & message : messages) {
            if (!message.is_object()) {
                throw std::invalid_argument("Expected 'message' to be an object, got " + message.dump());
            }

            common_chat_msg msg;
            if (!message.contains("role")) {
                throw std::invalid_argument("Missing 'role' in message: " + message.dump());
            }
            msg.role = message.at("role");

            auto has_content    = message.contains("content");
            auto has_tool_calls = message.contains("tool_calls");
            if (has_content) {
                const auto & content = message.at("content");
                if (content.is_string()) {
                    msg.content = content;
                } else if (content.is_array()) {
                    for (const auto & part : content) {
                        if (!part.contains("type")) {
                            throw std::invalid_argument("Missing content part type: " + part.dump());
                        }
                        const auto & type = part.at("type");
                        if (type != "text" && type != "media_marker") {
                            throw std::invalid_argument("Unsupported content part type: " + type.dump());
                        }
                        common_chat_msg_content_part msg_part;
                        msg_part.type = type;
                        msg_part.text = part.at("text");
                        msg.content_parts.push_back(msg_part);
                    }
                } else if (!content.is_null()) {
                    throw std::invalid_argument("Invalid 'content' type: expected string or array, got " +
                                                content.dump());
                }
            }
            if (has_tool_calls) {
                for (const auto & tool_call : message.at("tool_calls")) {
                    common_chat_tool_call tc;
                    if (!tool_call.contains("type")) {
                        throw std::invalid_argument("Missing tool call type: " + tool_call.dump());
                    }
                    const auto & type = tool_call.at("type");
                    if (type != "function") {
                        throw std::invalid_argument("Unsupported tool call type: " + tool_call.dump());
                    }
                    if (!tool_call.contains("function")) {
                        throw std::invalid_argument("Missing tool call function: " + tool_call.dump());
                    }
                    const auto & fc = tool_call.at("function");
                    if (!fc.contains("name")) {
                        throw std::invalid_argument("Missing tool call name: " + tool_call.dump());
                    }
                    tc.name           = fc.at("name");
                    const auto & args = fc.at("arguments");
                    if (args.is_string()) {
                        tc.arguments = args;
                    } else {
                        tc.arguments = args.dump();
                    }
                    if (tool_call.contains("id")) {
                        tc.id = tool_call.at("id");
                    }
                    msg.tool_calls.push_back(tc);
                }
            }
            if (!has_content && !has_tool_calls) {
                throw std::invalid_argument(
                    "Expected 'content' or 'tool_calls'");
            }
            if (message.contains("reasoning_content")) {
                msg.reasoning_content = message.at("reasoning_content");
            }
            if (message.contains("name")) {
                msg.tool_name = message.at("name");
            }
            if (message.contains("tool_call_id")) {
                msg.tool_call_id = message.at("tool_call_id");
            }

            msgs.push_back(msg);
        }
    } catch (const std::exception & e) {
        throw std::runtime_error("Failed to parse messages: " + std::string(e.what()));
    }

    return msgs;
}

json common_chat_msgs_to_json_oaicompat(const std::vector<common_chat_msg> & msgs, bool concat_typed_text) {
    json messages = json::array();
    for (const auto & msg : msgs) {
        messages.push_back(msg.to_json_oaicompat(concat_typed_text));
    }
    return messages;
}

json common_chat_tools_to_json_oaicompat(const std::vector<common_chat_tool> & tools) {
    if (tools.empty()) {
        return json();
    }

    auto result = json::array();
    for (const auto & tool : tools) {
        result.push_back({
            { "type",     "function" },
            { "function", {
                { "name", tool.name },
                { "description", tool.description },
                { "parameters", json::parse(tool.parameters) },
            }},
        });
    }
    return result;
}

std::vector<common_chat_tool> common_chat_tools_parse_oaicompat(const json & tools) {
    std::vector<common_chat_tool> result;

    try {
        if (!tools.is_array()) {
            throw std::invalid_argument("Expected 'tools' to be an array");
        }

        for (const auto & tool : tools) {
            if (!tool.contains("type") || tool.at("type") != "function") {
                throw std::invalid_argument("Unsupported tool type");
            }
            if (!tool.contains("function")) {
                throw std::invalid_argument("Missing 'function' in tool");
            }
            const auto & func = tool.at("function");
            common_chat_tool ct;
            if (func.contains("name")) {
                ct.name = func.at("name");
            }
            if (func.contains("description")) {
                ct.description = func.at("description");
            }
            if (func.contains("parameters")) {
                ct.parameters = func.at("parameters").dump();
            }
            result.push_back(ct);
        }
    } catch (const std::exception & e) {
        throw std::runtime_error("Failed to parse tools: " + std::string(e.what()));
    }

    return result;
}
