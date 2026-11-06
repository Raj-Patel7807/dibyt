#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <cstdint>

namespace dibyt::cli {

// Standardized, stable process exit codes
enum class ExitCode : int {
    kSuccess = 0,
    kUsageError = 1,
    kNetworkFailure = 2,
    kIntegrityFailure = 3,
    kStorageError = 4,
    kInterrupted = 130
};

struct CommandOptions {
    std::string command;
    std::string path;
    std::string name;
    std::string host{"127.0.0.1"};
    uint16_t port{9080};
    uint32_t workers{4};
    std::string dest_dir;
    bool json{false};
    bool quiet{false};
    bool verbose{false};
    bool overwrite{false};
    bool show_version{false};
    bool show_help{false};
};

// Parse command line arguments into structured options
[[nodiscard]] ExitCode parse_arguments(
    int argc, 
    char* argv[], 
    CommandOptions& out_opts, 
    std::string& out_error_msg
);

// Display top-level and subcommand help screens
void print_help(std::string_view subcommand = "");

// Render structured status output
void print_json_status(std::string_view status, std::string_view message);

} // namespace dibyt::cli
