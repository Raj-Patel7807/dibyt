#include "cli_parser.hpp"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include "dibyt/core/version.hpp"

namespace dibyt::cli {

ExitCode parse_arguments(
    int argc, 
    char* argv[], 
    CommandOptions& out_opts, 
    std::string& out_error_msg
) {
    if (argc <= 1) {
        out_opts.show_help = true;
        return ExitCode::kSuccess;
    }

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    // Top-level flags
    if (args[0] == "--version" || args[0] == "-v") {
        out_opts.show_version = true;
        return ExitCode::kSuccess;
    }

    if (args[0] == "--help" || args[0] == "-h" || args[0] == "help") {
        out_opts.show_help = true;
        if (args.size() > 1) {
            out_opts.command = args[1];
        }
        return ExitCode::kSuccess;
    }

    out_opts.command = args[0];

    for (size_t i = 1; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == "--json") {
            out_opts.json = true;
        } else if (arg == "--quiet" || arg == "-q") {
            out_opts.quiet = true;
        } else if (arg == "--verbose") {
            out_opts.verbose = true;
        } else if (arg == "--overwrite") {
            out_opts.overwrite = true;
        } else if (arg == "--path" && i + 1 < args.size()) {
            out_opts.path = args[++i];
        } else if (arg == "--name" && i + 1 < args.size()) {
            out_opts.name = args[++i];
        } else if (arg == "--dest" && i + 1 < args.size()) {
            out_opts.dest_dir = args[++i];
        } else if (arg == "--host" && i + 1 < args.size()) {
            out_opts.host = args[++i];
        } else if (arg == "--port" && i + 1 < args.size()) {
            out_opts.port = static_cast<uint16_t>(std::stoi(args[++i]));
        } else if (arg == "--workers" && i + 1 < args.size()) {
            out_opts.workers = static_cast<uint32_t>(std::stoi(args[++i]));
        } else {
            out_error_msg = "Unknown option or missing argument: " + arg;
            return ExitCode::kUsageError;
        }
    }

    // Validate subcommand requirements
    if (out_opts.command == "publish") {
        if (out_opts.path.empty()) {
            out_error_msg = "--path <directory> is required for 'publish'";
            return ExitCode::kUsageError;
        }
        if (out_opts.name.empty()) {
            out_opts.name = "default_collection";
        }
    } else if (out_opts.command == "fetch") {
        if (out_opts.dest_dir.empty()) {
            out_error_msg = "--dest <directory> is required for 'fetch'";
            return ExitCode::kUsageError;
        }
    } else if (out_opts.command == "status") {
        // Status does not require additional arguments
    } else {
        out_error_msg = "Unknown command: '" + out_opts.command + "'. Run 'dibyt --help' for available commands.";
        return ExitCode::kUsageError;
    }

    return ExitCode::kSuccess;
}

void print_help(std::string_view subcommand) {
    if (subcommand == "publish") {
        std::cout << "Usage: dibyt publish --path <dir> [--name <name>] [--json]\n\n"
                  << "Scan a local directory and generate an authoritative, immutable manifest.\n\n"
                  << "Options:\n"
                  << "  --path <path>    Local filesystem path to publish (required)\n"
                  << "  --name <name>    Logical collection name (default: default_collection)\n"
                  << "  --json           Format output as machine-readable JSON\n";
        return;
    }

    if (subcommand == "fetch") {
        std::cout << "Usage: dibyt fetch --dest <dir> [--path <src>] [--workers <N>] [--overwrite] [--json]\n\n"
                  << "Download a published file collection concurrently using parallel TCP streams.\n\n"
                  << "Options:\n"
                  << "  --dest <dir>     Destination root directory to write verified files (required)\n"
                  << "  --path <dir>     Source directory path (for local transfers)\n"
                  << "  --workers <N>    Number of parallel TCP workers (default: 4, max: 32)\n"
                  << "  --overwrite      Allow overwriting existing files in destination\n"
                  << "  --json           Format output as machine-readable JSON\n";
        return;
    }

    std::cout << "Dibyt — " << core::kProductTagline << "\n\n"
              << "Usage: dibyt <command> [options]\n\n"
              << "Commands:\n"
              << "  publish          Scan and publish an immutable file collection manifest\n"
              << "  fetch            Fetch a collection with parallel TCP streams and resume\n"
              << "  status           Inspect active transfers and daemon health\n"
              << "  help             Display help for a specific command\n\n"
              << "Global Options:\n"
              << "  -v, --version    Show version descriptors\n"
              << "  -h, --help       Show help options\n"
              << "      --json       Output results in structured JSON\n"
              << "      --quiet      Suppress non-essential progress lines\n"
              << "      --verbose    Enable detailed operational diagnostics\n";
}

void print_json_status(std::string_view status, std::string_view message) {
    std::cout << "{\n"
              << "  \"status\": \"" << status << "\",\n"
              << "  \"message\": \"" << message << "\"\n"
              << "}\n";
}

} // namespace dibyt::cli
