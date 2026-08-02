#include "arg-parser.h"
#include <iostream>

arg_parser::arg_parser(int argc, char ** argv)
    : argc_(argc), argv_(argv), empty_err_("") {}

bool arg_parser::has(const char* flag) const {
    if (pos_ >= static_cast<size_t>(argc_)) return false;
    return argv_[pos_] == flag;
}

bool arg_parser::consume(const char* flag) {
    if (has(flag)) {
        pos_++;
        return true;
    }
    return false;
}

bool arg_parser::has_value(const char* flag) const {
    if (!has(flag)) return false;
    return pos_ + 1 < static_cast<size_t>(argc_) && argv_[pos_ + 1][0] != '-';
}

std::optional<std::string> arg_parser::consume_value(const char* flag) {
    if (!consume(flag)) return std::nullopt;
    if (pos_ >= static_cast<size_t>(argc_)) return std::nullopt;
    if (argv_[pos_][0] == '-') {
        // The "value" looks like another flag — treat as boolean flag with no value
        return "";
    }
    return std::string(argv_[pos_++]);
}

std::string arg_parser::next() {
    if (pos_ >= static_cast<size_t>(argc_)) return "";
    return argv_[pos_++];
}

std::optional<std::string> arg_parser::get_value(const char* flag) const {
    for (size_t i = 1; i < static_cast<size_t>(argc_); i++) {
        if (argv_[i] == flag && i + 1 < static_cast<size_t>(argc_) && argv_[i+1][0] != '-') {
            return argv_[i+1];
        }
    }
    return std::nullopt;
}

bool arg_parser::handle(const char* flag, handler fn) {
    handlers_.emplace_back(flag, std::move(fn));
    return true;
}

void arg_parser::record_error(const std::string& msg) {
    error_ = msg;
}

std::string arg_parser::help_text() const {
    return "(no help registered)";
}

std::vector<std::string> arg_parser::remaining() const {
    std::vector<std::string> result;
    for (size_t i = pos_; i < static_cast<size_t>(argc_); i++) {
        result.push_back(argv_[i]);
    }
    return result;
}

bool arg_parser::parse() {
    while (pos_ < static_cast<size_t>(argc_)) {
        std::string arg = argv_[pos_];

        // Try each registered handler
        bool matched = false;
        for (auto& [flag, fn] : handlers_) {
            if (arg == flag) {
                // Check if this handler needs a value
                bool needs_value = false;
                for (size_t i = pos_ + 1; i < static_cast<size_t>(argc_); i++) {
                    if (argv_[i][0] == '-') break;
                    needs_value = true;
                    break;
                }

                if (needs_value) {
                    pos_++;
                    if (pos_ >= static_cast<size_t>(argc_)) {
                        record_error(flag + std::string(" requires a value"));
                        return false;
                    }
                    std::string value = argv_[pos_++];
                    if (!fn(value)) {
                        return false;
                    }
                } else {
                    pos_++;
                    if (!fn("")) {
                        return false;
                    }
                }
                matched = true;
                break;
            }
        }

        if (!matched) {
            // Unknown flag
            if (arg[0] == '-') {
                record_error("Unknown option: " + arg);
                return false;
            }
            // Otherwise treat as positional argument, skip it
            pos_++;
        }
    }

    return true;
}
