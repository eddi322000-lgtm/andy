#include "pdf-renderer.h"

#include "log.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cmath>
#include <unistd.h>
#include <cstdio>
#include <sys/wait.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// PDF rendering via pdftoppm CLI (fork/execvp — no shell, no injection)
// ---------------------------------------------------------------------------

static std::vector<uint8_t> render_page_to_png_pdftoppm(
    const std::string& pdf_path,
    int page_index,
    int width,
    int height) {
    
#if !defined(_WIN32)
    (void)height;
    // Calculate DPI from width (assuming 8.5 inch default width, 612 PDF points)
    int dpi = (width > 0) ? (width * 72 / 612) : 150;
    
    // Use mkstemp for a unique, safe output prefix
    char tmp_template[] = "/tmp/andy-agent-pdf-XXXXXX";
    int tmp_fd = mkstemp(tmp_template);
    if (tmp_fd < 0) {
        LOG_DBG("Failed to create temp file for PDF rendering");
        return {};
    }
    close(tmp_fd);
    std::string tmp_prefix = tmp_template;

    // Remove the temp file (mkstemp creates it); pdftoppm will create its own
    // files using the prefix as a base name.
    std::remove(tmp_prefix.c_str());

    // Build page-number file suffix: pdftoppm uses zero-padded page numbers
    int rendered_page = page_index + 1;
    std::string png_path = tmp_prefix + "-" +
                           (rendered_page < 10 ? "0" : "") + std::to_string(rendered_page) + ".png";

    // fork + execvp — no shell involved, so no command injection possible
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        std::string page_str = std::to_string(page_index + 1);
        std::string dpi_str = std::to_string(dpi);

        char dpi_arg[]   = "-r";
        char f_arg[]     = "-f";
        char l_arg[]     = "-l";
        char png_arg[]   = "-png";

        char * argv[] = {
            const_cast<char*>("pdftoppm"),
            png_arg,
            dpi_arg, const_cast<char*>(dpi_str.c_str()),
            f_arg,   const_cast<char*>(page_str.c_str()),
            l_arg,   const_cast<char*>(page_str.c_str()),
            const_cast<char*>(pdf_path.c_str()),
            const_cast<char*>(tmp_prefix.c_str()),
            nullptr
        };

        execvp("pdftoppm", argv);
        _exit(127);
    }

    if (pid < 0) {
        LOG_DBG("fork failed for pdftoppm");
        return {};
    }

    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        LOG_DBG("pdftoppm failed with exit status");
        std::remove(png_path.c_str());
        return {};
    }

    // Read the generated PNG file
    std::ifstream file(png_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_DBG("Failed to open generated PNG: %s", png_path.c_str());
        return {};
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> png_data(size);
    if (!file.read(reinterpret_cast<char*>(png_data.data()), size)) {
        LOG_DBG("Failed to read PNG data");
        file.close();
        return {};
    }
    
    file.close();
    
    // Clean up temporary file
    std::remove(png_path.c_str());
    
    return png_data;
#else
    (void)pdf_path;
    (void)page_index;
    (void)width;
    (void)height;
    LOG_DBG("PDF rendering not supported on Windows");
    return {};
#endif
}

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

std::vector<uint8_t> pdf_renderer::render_page_to_png(
    const std::string& pdf_path,
    int page_index,
    int width,
    int height) {
    
    // Use pdftoppm CLI (stable, no crash risk)
    return render_page_to_png_pdftoppm(pdf_path, page_index, width, height);
}

int pdf_renderer::get_page_count(const std::string& pdf_path) {
#if !defined(_WIN32)
    // Use pdfinfo via fork/execvp + pipes — no shell involved
    int stdout_pipe[2];
    if (pipe(stdout_pipe) < 0) {
        return 0;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child: run pdfinfo, write stdout to pipe
        close(stdout_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[1]);

        char * argv[] = {
            const_cast<char*>("pdfinfo"),
            const_cast<char*>(pdf_path.c_str()),
            nullptr
        };
        execvp("pdfinfo", argv);
        _exit(127);
    }

    if (pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return 0;
    }

    close(stdout_pipe[1]);

    // Read pdfinfo output
    std::string output;
    char buffer[256];
    ssize_t n;
    while ((n = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        output += buffer;
    }
    close(stdout_pipe[0]);

    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return 0;
    }

    // Parse "Pages: N" line
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("Pages:", 0) == 0) {
            try {
                return std::stoi(line.substr(6));
            } catch (...) {
                return 0;
            }
        }
    }
    return 0;
#else
    (void)pdf_path;
    return 0;
#endif
}

bool pdf_renderer::is_pdf(const std::string& path) {
    // Check file extension
    std::string ext = fs::path(path).extension().string();
    if (ext == ".pdf") {
        return true;
    }
    
    // Check PDF magic number (%PDF)
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    char header[5] = {0};
    file.read(header, 4);
    return std::strncmp(header, "%PDF", 4) == 0;
}