cmake_minimum_required(VERSION 3.14)
project(andy-agent-http LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cpp-httplib
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ---------------------------------------------------------------------------
# Source files (HTTP-only agent, no llama.cpp/ggml deps)
# ---------------------------------------------------------------------------
set(AGENT_SOURCES
    # Core common
    common/common.cpp
    common/chat.cpp
    common/chat-data.cpp
    common/log.cpp
    common/console.cpp
    common/peg-parser.cpp
    common/unicode.cpp
    common/json-schema-to-grammar.cpp

    # Jinja2 template engine
    common/jinja/caps.cpp
    common/jinja/lexer.cpp
    common/jinja/parser.cpp
    common/jinja/runtime.cpp
    common/jinja/string.cpp
    common/jinja/value.cpp

    # Agent tools
    tools/agent.cpp
    tools/arg-parser.cpp
    tools/command-dispatcher.cpp
    tools/command-executor.cpp
    tools/multimodal-input.cpp
    tools/agent-loop.cpp
    tools/agent-loop-compaction.cpp
    tools/agent-loop-completion-cli.cpp
    tools/agent-loop-completion.cpp
    tools/agent-loop-run.cpp
    tools/agent-loop-tools-cli.cpp
    tools/agent-loop-tools.cpp
    tools/http-inference-backend.cpp
    tools/agent-resources.cpp
    tools/clipboard-image.cpp
    tools/stb-image-impl.cpp
    tools/tool-registry.cpp
    tools/permission.cpp
    tools/permission-async.cpp
    tools/permission-policy.cpp
    tools/permission-prompt.cpp
    tools/compaction.cpp
    tools/session-file.cpp
    tools/agent-tool-parser.cpp

    # MCP
    tools/mcp/mcp-server-manager.cpp
    tools/mcp/mcp-tool-wrapper.cpp
    tools/mcp/mcp-client.cpp
    tools/mcp/mcp-server.cpp

    # Agents.md
    tools/agents-md/agents-md-manager.cpp

    # Skills
    tools/skills/skills-manager.cpp

    # PDF rendering (pdftoppm only, no Poppler-C++ dependency)
    tools/pdf-renderer.cpp

    # Built-in tools
    tools/tools/tool-bash.cpp
    tools/tools/tool-edit.cpp
    tools/tools/tool-glob.cpp
    tools/tools/tool-plan.cpp
    tools/tools/tool-read.cpp
    tools/tools/tool-write.cpp

    # Headers (for dependency tracking)
    tools/terminal-image.h
    tools/config-dir.h
    tools/arg-parser.h
    tools/command-dispatcher.h
    tools/command-executor.h
    tools/multimodal-input.h
    common/http.h
)

# ---------------------------------------------------------------------------
# PDF rendering uses pdftoppm CLI only (no Poppler-C++ library dependency)
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
add_executable(andy-agent ${AGENT_SOURCES})

# Link cpp-httplib
target_sources(andy-agent PRIVATE vendor/cpp-httplib/httplib.cpp)

# No Poppler-C++ linking (pdftoppm CLI only)

# Platform-specific flags
if(WIN32)
    target_compile_definitions(andy-agent PRIVATE _WIN32_WINNT=0x0A00)
elseif(APPLE)
    target_compile_options(andy-agent PRIVATE -fPIC)
elseif(UNIX)
    target_compile_options(andy-agent PRIVATE -fPIC)
    target_link_libraries(andy-agent pthread)
endif()

# ---------------------------------------------------------------------------
# Example: MCP server export mode
# ---------------------------------------------------------------------------
add_executable(andy-mcp-server
    tools/mcp/example-mcp-server.cpp
    tools/tool-registry.cpp
    tools/mcp/mcp-server.cpp
    common/log.cpp
    common/common.cpp
    tools/permission.cpp
    tools/permission-async.cpp
    tools/permission-policy.cpp
    tools/pdf-renderer.cpp
    tools/tools/tool-bash.cpp
    tools/tools/tool-read.cpp
    tools/tools/tool-write.cpp
    tools/tools/tool-edit.cpp
    tools/tools/tool-glob.cpp
    tools/tools/tool-plan.cpp
)

target_include_directories(andy-mcp-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cpp-httplib
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(andy-mcp-server pthread)

message(STATUS "andy-mcp-server: built (demonstrates MCP server export mode)")
