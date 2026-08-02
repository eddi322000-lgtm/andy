#include "server-context.h"

#ifdef ANDY_AGENT_SERVER
// When building the server with llama.cpp headers:
// #include "llama.h"
// #include "common.h"
#endif

#include <cstdio>
#include <iostream>

server_context::server_context() = default;
server_context::~server_context() {
    terminate();
}

bool server_context::load_model(const common_params & params) {
#ifdef ANDY_AGENT_SERVER
    // TODO: Implement actual llama.cpp model loading
    // This requires llama.cpp headers and the full common_params struct
    //
    // Example implementation:
    // auto model = llama_load_model_from_file(params.model.path.c_str(), params.model.params);
    // if (!model) { return false; }
    // model_ = model;
    //
    // auto ctx = llama_new_context_with_model(model, params.ctx_params);
    // if (!ctx) { return false; }
    // llama_ctx_ = ctx;
    // model_loaded_ = true;
    // return true;
#else
    (void)params;
    fprintf(stderr, "ERROR: server_context::load_model called without ANDY_AGENT_SERVER\n");
    return false;
#endif
}

void server_context::terminate() {
    running_ = false;
#ifdef ANDY_AGENT_SERVER
    // TODO: llama_free((llama_context *)llama_ctx_);
    // llama_free_model((llama_model *)model_);
    llama_ctx_ = nullptr;
    model_ = nullptr;
    model_loaded_ = false;
#endif
}

void server_context::start_loop() {
#ifdef ANDY_AGENT_SERVER
    // TODO: Implement the main inference loop
    // This would wrap llama_server_context::process_tokens() and similar
    running_ = true;
    // while (running_) {
    //     // Process requests from the queue
    //     // Generate tokens
    //     // Send SSE responses
    // }
#else
    fprintf(stderr, "ERROR: server_context::start_loop called without ANDY_AGENT_SERVER\n");
#endif
}

bool server_context::is_valid() const {
    return model_loaded_;
}
