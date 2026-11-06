#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include "cli_parser.hpp"

// Unit test suite for CLI argument parser, option validation, and exit codes
int main() {
    std::cout << "[TEST] Running CLI Parser & Product Experience Tests...\n";

    // 1. Version flag parsing
    {
        char* argv[] = {const_cast<char*>("dibyt"), const_cast<char*>("--version")};
        dibyt::cli::CommandOptions opts;
        std::string err;
        assert(dibyt::cli::parse_arguments(2, argv, opts, err) == dibyt::cli::ExitCode::kSuccess);
        assert(opts.show_version);
    }

    // 2. Publish command validation
    {
        char* argv[] = {
            const_cast<char*>("dibyt"), 
            const_cast<char*>("publish"), 
            const_cast<char*>("--path"), 
            const_cast<char*>("/tmp/data"),
            const_cast<char*>("--name"),
            const_cast<char*>("dataset_v1"),
            const_cast<char*>("--json")
        };
        dibyt::cli::CommandOptions opts;
        std::string err;
        assert(dibyt::cli::parse_arguments(7, argv, opts, err) == dibyt::cli::ExitCode::kSuccess);
        assert(opts.command == "publish");
        assert(opts.path == "/tmp/data");
        assert(opts.name == "dataset_v1");
        assert(opts.json);
    }

    // 3. Publish missing required --path argument
    {
        char* argv[] = {const_cast<char*>("dibyt"), const_cast<char*>("publish")};
        dibyt::cli::CommandOptions opts;
        std::string err;
        assert(dibyt::cli::parse_arguments(2, argv, opts, err) == dibyt::cli::ExitCode::kUsageError);
        assert(!err.empty());
    }

    // 4. Unknown subcommand error
    {
        char* argv[] = {const_cast<char*>("dibyt"), const_cast<char*>("magic_command")};
        dibyt::cli::CommandOptions opts;
        std::string err;
        assert(dibyt::cli::parse_arguments(2, argv, opts, err) == dibyt::cli::ExitCode::kUsageError);
    }

    std::cout << "[SMOKE_TEST_PASSED] CLI parsing and validation logic verified successfully.\n";
    return 0;
}
