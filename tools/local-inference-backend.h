#pragma once

#include "inference-backend.h"

// Forward declarations
struct common_params;
class server_context;

// Local inference backend — wraps llama.cpp in-process inference
// Implements the inference_backend interface for local model execution
class local_inference_backend : public inference_backend {
public:
    explicit local_inference_backend(server_context & ctx, const common_params & params);
    ~local_inference_backend() override = default;

    const inference_backend_meta & meta() const override;

    inference_result complete(
        const inference_request & request,
        std::function<void(const inference_event &)> on_event,
        std::function<bool()> should_stop) override;

private:
    server_context & ctx_;
    common_params params_;
    inference_backend_meta meta_;
};
