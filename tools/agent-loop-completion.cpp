#include "agent-loop.h"
#include "console.h"

#include <functional>

// Streaming version of generate_completion
inference_result agent_loop::generate_completion_streaming(
    agent_event_callback on_event,
    std::function<bool()> should_stop) {

    auto chat_tools = tool_registry::instance().to_chat_tools();
    inference_request request = build_inference_request(chat_tools);

    inference_result result = backend_.complete(
        request,
        [this, on_event](const inference_event & event) {
            switch (event.type) {
                case inference_event_type::TEXT_DELTA:
                    if (!event.diff.content_delta.empty()) {
                        on_event(agent_event::text_delta(event.diff.content_delta));
                    }
                    break;
                case inference_event_type::REASONING_DELTA:
                    if (!event.diff.reasoning_content_delta.empty()) {
                        on_event(agent_event::reasoning_delta(event.diff.reasoning_content_delta));
                    }
                    break;
                case inference_event_type::ERROR:
                    if (!event.error.empty()) {
                        on_event(agent_event::error(event.error));
                    }
                    break;
                case inference_event_type::PROMPT_PROGRESS: {
                    // Update status bar with context fill info
                    int32_t total = event.progress.total;
                    int32_t cache = event.progress.cache;
                    int32_t processed = event.progress.processed;
                    if (n_ctx_ > 0 && total > 0) {
                        int32_t fill_pct = (processed * 100) / total;
                        int32_t cache_pct = (cache * 100) / total;
                        console::status_bar([fill_pct, cache_pct, total, cache](std::string & out) {
                            out = "🦙 ctx: " + std::to_string(total) + " tokens | "
                                + std::to_string(fill_pct) + "% ("
                                + std::to_string(cache_pct) + "% cached)";
                        });
                    }
                    break;
                }
                case inference_event_type::TOOL_CALL_DELTA:
                    break;
            }
        },
        should_stop);

    last_prompt_tokens_ = result.prompt_tokens;
    last_completion_overflowed_ = result.context_overflow;
    return result;
}
