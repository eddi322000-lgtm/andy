#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Include agent-loop.h for json typedef (nlohmann::ordered_json)
#include "agent-loop.h"

// Represents a pasted image from clipboard
struct image_entry {
    std::vector<uint8_t> bytes;
    std::string mime_type;
};

// Handle multimodal input: images pasted via Ctrl+V.
// Builds JSON content for the LLM API and renders terminal previews.
class multimodal_input {
public:
    // Build JSON content from text + images.
    // If has_vision is true and images are present, builds a content array
    // with text and image_url blocks. Otherwise returns plain text.
    // Returns the JSON content (string or array).
    static json build_content(const std::string& text,
                              const std::vector<image_entry>& images,
                              bool has_vision);

    // Render terminal previews for pasted images.
    // Returns true if any images were rendered.
    static bool render_previews(const std::vector<image_entry>& images);

    // Strip [image] / [image N] markers from text (used for display only).
    static std::string strip_markers(const std::string& text,
                                     size_t num_images);

    // Check if model can handle images.
    static bool can_show_images(const std::vector<image_entry>& images,
                                bool has_vision);

    // Convert PDF file to PNG images (for vision LLM).
    // Returns vector of PNG image entries, one per page (max 10 pages).
    static std::vector<image_entry> convert_pdf_to_images(const std::string& pdf_path);
};
