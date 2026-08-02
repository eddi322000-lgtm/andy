// Chat support (stripped for HTTP-only agent mode)

#pragma once

#include "common.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

using json = nlohmann::ordered_json;

// Forward declarations for types that may be referenced
struct common_chat_templates;
struct llama_model;
namespace autoparser {
struct generation_params;
}  // namespace autoparser

// ---------------------------------------------------------------------------
// Chat data structs (used by inference-backend.h and agent-loop.h)
// ---------------------------------------------------------------------------

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

    nlohmann::ordered_json to_json_oaicompat(bool concat_typed_text = false) const;
    std::string render_content(const std::string & delimiter = "\n\n") const;

    bool empty() const {
        return content.empty() && content_parts.empty() && tool_calls.empty() &&
               reasoning_content.empty() && tool_name.empty() && tool_call_id.empty();
    }

    bool contains_media() const {
        for (const auto & part : content_parts) {
            if (part.type == "media_marker") return true;
        }
        return false;
    }

    void set_tool_call_ids(std::vector<std::string> &           ids_cache,
                           const std::function<std::string()> & gen_tool_call_id) {
        for (auto i = 0u; i < tool_calls.size(); i++) {
            if (ids_cache.size() <= i) {
                auto id = tool_calls[i].id;
                if (id.empty()) id = gen_tool_call_id();
                ids_cache.push_back(id);
            }
            tool_calls[i].id = ids_cache[i];
        }
    }

    bool operator==(const common_chat_msg & other) const {
        return role == other.role && content == other.content &&
               content_parts == other.content_parts && tool_calls == other.tool_calls &&
               reasoning_content == other.reasoning_content &&
               tool_name == other.tool_name && tool_call_id == other.tool_call_id;
    }

    bool operator!=(const common_chat_msg & other) const { return !(*this == other); }
};

struct common_chat_msg_diff {
    std::string           reasoning_content_delta;
    std::string           content_delta;
    size_t                tool_call_index = std::string::npos;
    common_chat_tool_call tool_call_delta;

    static std::vector<common_chat_msg_diff> compute_diffs(const common_chat_msg & msg_prv,
                                                           const common_chat_msg & msg_new);
};

struct common_chat_tool {
    std::string name;
    std::string description;
    std::string parameters;
};

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum common_chat_tool_choice {
    COMMON_CHAT_TOOL_CHOICE_AUTO,
    COMMON_CHAT_TOOL_CHOICE_REQUIRED,
    COMMON_CHAT_TOOL_CHOICE_NONE,
};

enum common_chat_format {
    COMMON_CHAT_FORMAT_CONTENT_ONLY,
    COMMON_CHAT_FORMAT_PEG_SIMPLE,
    COMMON_CHAT_FORMAT_PEG_NATIVE,
    COMMON_CHAT_FORMAT_PEG_GEMMA4,
    COMMON_CHAT_FORMAT_COUNT,
};

enum common_chat_continuation {
    COMMON_CHAT_CONTINUATION_NONE,
    COMMON_CHAT_CONTINUATION_AUTO,
    COMMON_CHAT_CONTINUATION_REASONING,
    COMMON_CHAT_CONTINUATION_CONTENT,
};

// ---------------------------------------------------------------------------
// Chat params structs (used by agent-loop.h and inference-backend.h)
// ---------------------------------------------------------------------------

struct common_chat_templates_inputs {
    std::vector<common_chat_msg>          messages;
    std::string                           grammar;
    std::string                           json_schema;
    bool                                  add_generation_prompt  = true;
    common_chat_continuation              continue_final_message = COMMON_CHAT_CONTINUATION_NONE;
    bool                                  use_jinja              = true;
    std::vector<common_chat_tool>         tools;
    common_chat_tool_choice               tool_choice         = COMMON_CHAT_TOOL_CHOICE_AUTO;
    bool                                  parallel_tool_calls = false;
    common_reasoning_format               reasoning_format    = COMMON_REASONING_FORMAT_NONE;
    bool                                  enable_thinking     = true;
    std::chrono::system_clock::time_point now                 = std::chrono::system_clock::now();
    std::map<std::string, std::string>    chat_template_kwargs;
    bool                                  add_bos = false;
    bool                                  add_eos = false;
    bool                                  force_pure_content = false;
};

struct common_chat_params {
    common_chat_format                  format = COMMON_CHAT_FORMAT_CONTENT_ONLY;
    std::string                         prompt;
    std::string                         grammar;
    bool                                grammar_lazy         = false;
    std::string                         generation_prompt;
    bool                                supports_thinking    = false;
    std::string                         thinking_start_tag;
    std::string                         thinking_end_tag;
    std::vector<common_grammar_trigger> grammar_triggers;
    std::vector<std::string>            preserved_tokens;
    std::vector<std::string>            additional_stops;
    std::string                         parser;
};

// ---------------------------------------------------------------------------
// JSON parsing/serialization functions (implemented in chat-data.cpp)
// ---------------------------------------------------------------------------

std::vector<common_chat_msg> common_chat_msgs_parse_oaicompat(const json & messages);
json common_chat_msgs_to_json_oaicompat(const std::vector<common_chat_msg> & msgs, bool concat_typed_text = false);
std::vector<common_chat_tool> common_chat_tools_parse_oaicompat(const json & tools);
json common_chat_tools_to_json_oaicompat(const std::vector<common_chat_tool> & tools);

// ---------------------------------------------------------------------------
// Utility functions (minimal implementations)
// ---------------------------------------------------------------------------

const char *    common_chat_format_name(common_chat_format format);
common_chat_tool_choice common_chat_tool_choice_parse_oaicompat(const std::string & tool_choice);
common_chat_continuation common_chat_continuation_parse(const json & value);
const char *    common_reasoning_format_name(common_reasoning_format format);
common_reasoning_format common_reasoning_format_from_name(const std::string & format);
