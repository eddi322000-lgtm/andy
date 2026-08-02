// Chat data structures and JSON serialization (HTTP-only, no llama.cpp/ggml deps)

#pragma once

#include "jinja/caps.h"
#include "nlohmann/json_fwd.hpp"

#include <string>
#include <vector>
#include <functional>

using json = nlohmann::ordered_json;

struct common_chat_tool_call {
    std::string name;
    std::string arguments;
    std::string id;

    bool operator==(const common_chat_tool_call & other) const {
        return name == other.name && arguments == other.arguments && id == other.id;
    }
};

struct common_chat_msg_content_part {
    std::string type;
    std::string text;

    bool operator==(const common_chat_msg_content_part & other) const {
        return type == other.type && text == other.text;
    }
};

struct common_chat_msg {
    std::string                               role;
    std::string                               content;
    std::vector<common_chat_msg_content_part> content_parts;
    std::vector<common_chat_tool_call>        tool_calls;
    std::string                               reasoning_content;
    std::string                               tool_name;
    std::string                               tool_call_id;

    json to_json_oaicompat(bool concat_typed_text = false) const;

    std::string render_content(const std::string & delimiter = "\n\n") const;

    bool empty() const {
        return content.empty() && content_parts.empty() && tool_calls.empty() &&
               reasoning_content.empty() && tool_name.empty() && tool_call_id.empty();
    }

    bool contains_media() const {
        for (const auto & part : content_parts) {
            if (part.type == "media_marker") {
                return true;
            }
        }
        return false;
    }

    void set_tool_call_ids(std::vector<std::string> &           ids_cache,
                           const std::function<std::string()> & gen_tool_call_id) {
        for (auto i = 0u; i < tool_calls.size(); i++) {
            if (ids_cache.size() <= i) {
                auto id = tool_calls[i].id;
                if (id.empty()) {
                    id = gen_tool_call_id();
                }
                ids_cache.push_back(id);
            }
            tool_calls[i].id = ids_cache[i];
        }
    }

    bool operator==(const common_chat_msg & other) const {
        return role == other.role && content == other.content && content_parts == other.content_parts &&
               tool_calls == other.tool_calls && reasoning_content == other.reasoning_content &&
               tool_name == other.tool_name && tool_call_id == other.tool_call_id;
    }

    bool operator!=(const common_chat_msg & other) const { return !(*this == other); }
};

struct common_chat_tool {
    std::string name;
    std::string description;
    std::string parameters;
};

// Parses a JSON array of messages in OpenAI's chat completion API format.
std::vector<common_chat_msg> common_chat_msgs_parse_oaicompat(const json & messages);

// Serialize messages to OpenAI-compatible JSON format.
json common_chat_msgs_to_json_oaicompat(const std::vector<common_chat_msg> & msgs, bool concat_typed_text = false);

// Serialize tools to OpenAI-compatible JSON format.
json common_chat_tools_to_json_oaicompat(const std::vector<common_chat_tool> & tools);

// Parse tools from OpenAI-compatible JSON format.
std::vector<common_chat_tool> common_chat_tools_parse_oaicompat(const json & tools);
