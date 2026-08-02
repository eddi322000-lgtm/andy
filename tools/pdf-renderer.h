#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Render PDF pages to PNG images
// Uses poppler-cpp or stb_image-based PDF rendering
class pdf_renderer {
public:
    // Render a single PDF page to PNG bytes
    // page_index: 0-based page number
    // Returns PNG bytes and mime_type, or empty on failure
    static std::vector<uint8_t> render_page_to_png(
        const std::string& pdf_path,
        int page_index,
        int width = 1920,
        int height = 0  // 0 = auto-scale to maintain aspect ratio
    );

    // Get number of pages in PDF
    static int get_page_count(const std::string& pdf_path);

    // Check if file is a PDF
    static bool is_pdf(const std::string& path);
};
