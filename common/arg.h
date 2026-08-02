#pragma once

#include <set>
#include <map>
#include <string>
#include <vector>
#include <cstring>

// pseudo-env variable to identify preset-only arguments
#define COMMON_ARG_PRESET_LOAD_ON_STARTUP "__PRESET_LOAD_ON_STARTUP"
#define COMMON_ARG_PRESET_STOP_TIMEOUT    "__PRESET_STOP_TIMEOUT"

//
// CLI argument parsing (stripped for HTTP-only agent mode)
//

// Forward declarations (stub types for HTTP-only mode)
struct llama_example;

struct common_arg {
    std::set<int> examples;
    std::set<int> excludes;
    std::vector<const char *> args;
    std::vector<const char *> args_neg;  // for negated args like --no-xxx
    const char * value_hint   = nullptr; // help text or example for arg value
    const char * value_hint_2 = nullptr; // for second arg value
    const char * env          = nullptr;
    std::string help;
    bool is_sampling = false; // is current arg a sampling param?
    bool is_spec = false; // is current arg a speculative decoding param?
    bool is_preset_only = false; // is current arg preset-only (not treated as CLI arg)
    void (*handler_void)   (void * params) = nullptr;
    void (*handler_string) (void * params, const std::string &) = nullptr;
    void (*handler_str_str)(void * params, const std::string &, const std::string &) = nullptr;
    void (*handler_int)    (void * params, int) = nullptr;
    void (*handler_bool)   (void * params, bool) = nullptr;

    common_arg() = default;

    common_arg(
        const std::initializer_list<const char *> & args,
        const char * value_hint,
        const std::string & help,
        void (*handler)(void * params, const std::string &)
    ) : args(args), value_hint(value_hint), help(help), handler_string(handler) {}

    common_arg(
        const std::initializer_list<const char *> & args,
        const char * value_hint,
        const std::string & help,
        void (*handler)(void * params, int)
    ) : args(args), value_hint(value_hint), help(help), handler_int(handler) {}

    common_arg(
        const std::initializer_list<const char *> & args,
        const std::string & help,
        void (*handler)(void * params)
    ) : args(args), help(help), handler_void(handler) {}

    common_arg(
        const std::initializer_list<const char *> & args,
        const std::initializer_list<const char *> & args_neg,
        const std::string & help,
        void (*handler)(void * params, bool)
    ) : args(args), args_neg(args_neg), help(help), handler_bool(handler) {}

    // support 2 values for arg
    common_arg(
        const std::initializer_list<const char *> & args,
        const char * value_hint,
        const char * value_hint_2,
        const std::string & help,
        void (*handler)(void * params, const std::string &, const std::string &)
    ) : args(args), value_hint(value_hint), value_hint_2(value_hint_2), help(help), handler_str_str(handler) {}

    common_arg & set_examples(std::initializer_list<int> examples);
    common_arg & set_excludes(std::initializer_list<int> excludes);
    common_arg & set_env(const char * env);
    common_arg & set_sampling();
    common_arg & set_spec();
    common_arg & set_preset_only();
    bool in_example(int ex);
    bool is_exclude(int ex);
    bool get_value_from_env(std::string & output) const;
    bool has_value_from_env() const;
    std::string to_string() const;

    // for using as key in std::map
    bool operator<(const common_arg& other) const {
        if (args.empty() || other.args.empty()) {
            return false;
        }
        return strcmp(args[0], other.args[0]) < 0;
    }
    bool operator==(const common_arg& other) const {
        if (args.empty() || other.args.empty()) {
            return false;
        }
        return strcmp(args[0], other.args[0]) == 0;
    }

    // get all args and env vars (including negated args/env)
    std::vector<std::string> get_args() const;
    std::vector<std::string> get_env() const;
};

namespace common_arg_utils {
    bool is_truthy(const std::string & value);
    bool is_falsey(const std::string & value);
    bool is_autoy(const std::string & value);
}

// parse input arguments from CLI
bool common_params_parse(int argc, char ** argv, void * params, int ex, void(*print_usage)(int, char **) = nullptr);

// parse input arguments from CLI into a map
bool common_params_to_map(int argc, char ** argv, int ex, std::map<common_arg, std::string> & out_map);

// populate preset-only arguments
void common_params_add_preset_options(std::vector<common_arg> & args);

// initialize argument parser context
void * common_params_parser_init(void * params, int ex, void(*print_usage)(int, char **) = nullptr);
