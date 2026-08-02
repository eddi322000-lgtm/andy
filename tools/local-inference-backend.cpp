#include "local-inference-backend.h"
#include "../server/server-context.h"

#include <chrono>
#include <iostream>

local_inference_backend::local_inference_backend(server_context & ctx, const common_params & params)
    : ctx_(ctx)
    , params_(params) {
    // Initialize metadata from params
    meta_.model_name = params_.model.name.empty() ? "local" : params_.model.name;
    meta_.n_ctx = params_.ctx_params.n_ctx;
    meta_.has_vision = params_.vision;
    meta_.has_audio = false;
    meta_.is_llama_server = false;
    meta_.total_slots = 0;  // Local backend doesn't use slots
    meta_.build_info = "local-inference";
}

const inference_backend_meta & local_inference_backend::meta() const {
    return meta_;
}

inference_result local_inference_backend::complete(
    const inference_request & request,
    std::function<void(const inference_event &)> on_event,
    std::function<bool()> should_stop) {

    inference_result result;

    if (!ctx_.is_valid()) {
        result.error = "Model not loaded";
        result.cancelled = true;
        return result;
    }

    // TODO: Implement actual llama.cpp inference
    // This requires:
    // 1. Converting inference_request messages to llama prompt format
    // 2. Calling llama_decode() in a loop
    // 3. Sampling tokens with llama_sample_*** functions
    // 4. Converting tokens back to text deltas
    // 5. Handling tool calls via grammar or function calling
    //
    // The existing http-inference-backend.cpp shows the event structure:
    //   - TEXT_DELTA: streaming token output
    //   - REASONING_DELTA: reasoning/thinking content
    //   - TOOL_CALL_DELTA: tool call arguments
    //   - PROMPT_PROGRESS: KV cache progress
    //   - ERROR: errors

    (void)request;
    (void)on_event;
    (void)should_stop;

    // Stub: return empty result until full implementation
    result.cancelled = true;
    result.error = "Local inference backend not yet implemented — use --backend http with external llama-server";
    return result;
}
