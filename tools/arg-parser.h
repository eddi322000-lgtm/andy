#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

// Lightweight argument parser that does NOT mutate argc/argv.
// Scans forward through argv, supports both --flag and --flag value patterns.
// Usage:
//   arg_parser parser(argc, argv);
//   if (parser.has("--url")) {
//       params.url = parser.next_value();
//   }
//   if (parser.has("--yolo")) {
//       params.yolo_mode = true;
//   }

class arg_parser {
public:
    using handler = std::function<bool(const std::string& value)>;

    explicit arg_parser(int argc, char ** argv);

    // Check if the current argument matches a flag (without consuming it).
    // Returns true if argv[pos] == flag.
    bool has(const char* flag) const;

    // Check if the current argument matches a flag AND consume it.
    // Returns true and advances position if matched.
    bool consume(const char* flag);

    // Check if the current position has a value for a flag.
    // Does NOT consume the flag or the value.
    bool has_value(const char* flag) const;

    // Consume a flag and return its value (if present).
    // Returns the value string, or std::nullopt if no value follows.
    std::optional<std::string> consume_value(const char* flag);

    // Consume the current argument and return it.
    // Returns empty string if already at end.
    std::string next();

    // Get the value for a flag without consuming the flag itself.
    // Useful for one-shot parsing.
    std::optional<std::string> get_value(const char* flag) const;

    // Register a handler for a flag. The handler receives the flag's value
    // (or empty string for boolean flags) and returns true to continue parsing.
    bool handle(const char* flag, handler fn);

    // Run the full parse loop. Returns false if parsing failed (unknown flag,
    // missing required value, etc.).
    bool parse();

    // Get help text for registered flags.
    std::string help_text() const;

    // Get the remaining unconsumed arguments.
    std::vector<std::string> remaining() const;

    // Check if parsing succeeded (no errors encountered).
    bool succeeded() const { return !error_.has_value(); }

    // Get the last error message, if any.
    std::string error() const { return error_.value_or(empty_err_); }

private:
    int argc_;
    char ** argv_;
    size_t pos_ = 0;
    std::vector<std::pair<std::string, handler>> handlers_;
    std::optional<std::string> error_;
    std::string empty_err_;

    void record_error(const std::string& msg);
};
