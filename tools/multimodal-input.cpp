#include "multimodal-input.h"
#include "pdf-renderer.h"
#include "terminal-image.h"
#include "base64.hpp"
#include "log.h"
#include "nlohmann/json.hpp"

#include <filesystem>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// PDF auto-detection and conversion
// ---------------------------------------------------------------------------

std::vector<image_entry> multimodal_input::convert_pdf_to_images(const std::string& pdf_path) {
    std::vector<image_entry> images;
    
    if (!pdf_renderer::is_pdf(pdf_path)) {
        return images;
    }
    
    int page_count = pdf_renderer::get_page_count(pdf_path);
    if (page_count <= 0) {
        LOG_DBG("Failed to get page count for PDF: %s", pdf_path.c_str());
        return images;
    }
    
    LOG_DBG("Converting PDF to %d PNG page(s): %s", page_count, pdf_path.c_str());
    
    // Render all pages (limit to first 10 pages to avoid overwhelming the LLM)
    int max_pages = std::min(page_count, 10);
    for (int i = 0; i < max_pages; i++) {
        auto png_data = pdf_renderer::render_page_to_png(pdf_path, i, 1920, 0);
        if (!png_data.empty()) {
            image_entry entry;
            entry.bytes = std::move(png_data);
            entry.mime_type = "image/png";
            images.push_back(std::move(entry));
        }
    }
    
    return images;
}

// ---------------------------------------------------------------------------
// Build content with PDF support
// ---------------------------------------------------------------------------

json multimodal_input::build_content(const std::string& text,
                                     const std::vector<image_entry>& images,
                                     bool has_vision) {
    if (images.empty()) {
        return text;
    }

    if (!can_show_images(images, has_vision)) {
        return text;
    }

    // Render terminal previews
    render_previews(images);

    // Strip [image] markers from text
    std::string clean_text = strip_markers(text, images.size());

    // Build content array
    json content = json::array();
    if (!clean_text.empty()) {
        content.push_back({{"type", "text"}, {"text", clean_text}});
    }

    for (const auto& [bytes, mime] : images) {
        std::string b64 = base64::encode(
            reinterpret_cast<const char*>(bytes.data()), bytes.size());
        content.push_back({
            {"type", "image_url"},
            {"image_url", {{"url", "data:" + mime + ";base64," + b64}}}
        });
    }

    return content;
}

bool multimodal_input::render_previews(const std::vector<image_entry>& images) {
    for (const auto& [bytes, mime] : images) {
        render_image_to_terminal(bytes.data(), bytes.size(), mime);
    }
    return !images.empty();
}

std::string multimodal_input::strip_markers(const std::string& text,
                                            size_t num_images) {
    std::string result = text;
    for (size_t n = num_images; n >= 1; n--) {
        std::string marker = n == 1 ? "[image]" : "[image " + std::to_string(n) + "]";
        size_t pos = result.find(marker);
        if (pos != std::string::npos) {
            result.erase(pos, marker.size());
        }
    }
    // Trim whitespace left by marker removal
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

bool multimodal_input::can_show_images(const std::vector<image_entry>& images,
                                       bool has_vision) {
    if (images.empty()) return true;
    if (!has_vision) return false;
    return true;
}
