#pragma once

// Forward declarations for llama.cpp types
struct llama_model;
struct llama_context;
struct common_params;

// Opaque server context — wraps llama.cpp inference
// This header is only included when ANDY_AGENT_SERVER is defined
// and llama.cpp headers are available.

class server_context {
public:
    server_context();
    ~server_context();

    // Load a model from file
    bool load_model(const common_params & params);

    // Terminate the inference loop
    void terminate();

    // Start the main inference loop (blocking)
    void start_loop();

    // Check if the context is valid (model loaded)
    bool is_valid() const;

private:
    void * llama_ctx_ = nullptr;
    void * model_ = nullptr;
    bool running_ = false;
    bool model_loaded_ = false;
};
