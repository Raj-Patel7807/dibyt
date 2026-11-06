#include <iostream>
#include <filesystem>
#include "cli_parser.hpp"
#include "dibyt/core/version.hpp"
#include "dibyt/manifest/manifest.hpp"
#include "dibyt/storage/database.hpp"
#include "dibyt/transfer/engine.hpp"

// Thin CLI main dispatcher: parses arguments and delegates execution to library subsystems
int main(int argc, char* argv[]) {
    dibyt::cli::CommandOptions opts;
    std::string err_msg;

    auto exit_code = dibyt::cli::parse_arguments(argc, argv, opts, err_msg);
    if (exit_code != dibyt::cli::ExitCode::kSuccess) {
        std::cerr << "Error: " << err_msg << "\n";
        return static_cast<int>(exit_code);
    }

    if (opts.show_version) {
        if (opts.json) {
            std::cout << "{\n"
                      << "  \"product\": \"" << dibyt::core::kProductName << "\",\n"
                      << "  \"version\": \"" << dibyt::core::kProductVersion << "\"\n"
                      << "}\n";
        } else {
            std::cout << dibyt::core::kProductName << " v" << dibyt::core::kProductVersion << " — "
                      << dibyt::core::kProductTagline << "\n";
        }
        return static_cast<int>(dibyt::cli::ExitCode::kSuccess);
    }

    if (opts.show_help) {
        dibyt::cli::print_help(opts.command);
        return static_cast<int>(dibyt::cli::ExitCode::kSuccess);
    }

    // Command Dispatcher
    if (opts.command == "publish") {
        dibyt::manifest::ScannerOptions scan_opts;
        dibyt::manifest::Manifest manifest;
        auto scan_err = dibyt::manifest::build_collection_manifest(
            opts.path, opts.name, scan_opts, manifest
        );

        if (scan_err != dibyt::core::ErrorCode::kOk) {
            std::cerr << "Publish failed: " << dibyt::core::error_code_to_string(scan_err) << "\n";
            return static_cast<int>(dibyt::cli::ExitCode::kStorageError);
        }

        if (opts.json) {
            std::cout << "{\n"
                      << "  \"collection\": \"" << manifest.collection_name << "\",\n"
                      << "  \"files_count\": " << manifest.files.size() << ",\n"
                      << "  \"total_bytes\": " << manifest.total_size_bytes << "\n"
                      << "}\n";
        } else {
            std::cout << "Published collection '" << manifest.collection_name << "': "
                      << manifest.files.size() << " files (" 
                      << manifest.total_size_bytes << " bytes)\n";
        }
        return static_cast<int>(dibyt::cli::ExitCode::kSuccess);
    }

    if (opts.command == "fetch") {
        if (opts.path.empty()) {
            std::cerr << "Error: --path <source_dir> is currently required for local fetch.\n";
            return static_cast<int>(dibyt::cli::ExitCode::kUsageError);
        }

        dibyt::manifest::ScannerOptions scan_opts;
        dibyt::manifest::Manifest manifest;
        auto scan_err = dibyt::manifest::build_collection_manifest(
            opts.path, "fetch_job", scan_opts, manifest
        );
        if (scan_err != dibyt::core::ErrorCode::kOk) {
            std::cerr << "Scan failed: " << dibyt::core::error_code_to_string(scan_err) << "\n";
            return static_cast<int>(dibyt::cli::ExitCode::kStorageError);
        }

        std::filesystem::path db_path = std::filesystem::path(opts.dest_dir) / ".dibyt-staging" / "meta.db";
        dibyt::storage::Database db;
        (void)db.open(db_path);

        dibyt::transfer::EngineConfig config;
        config.num_workers = opts.workers;
        config.overwrite_policy = opts.overwrite 
            ? dibyt::transfer::OverwritePolicy::kOverwrite 
            : dibyt::transfer::OverwritePolicy::kDisallow;

        dibyt::transfer::TransferEngine engine(db, config);
        auto transfer_err = engine.execute_local_parallel_transfer(opts.path, opts.dest_dir, manifest);

        db.close();

        if (transfer_err != dibyt::core::ErrorCode::kOk) {
            std::cerr << "Transfer failed: " << dibyt::core::error_code_to_string(transfer_err) << "\n";
            return static_cast<int>(dibyt::cli::ExitCode::kIntegrityFailure);
        }

        if (opts.json) {
            std::cout << "{\n"
                      << "  \"status\": \"completed\",\n"
                      << "  \"files_transferred\": " << manifest.files.size() << ",\n"
                      << "  \"destination\": \"" << opts.dest_dir << "\"\n"
                      << "}\n";
        } else {
            std::cout << "Transfer complete: " << manifest.files.size() 
                      << " files written to " << opts.dest_dir << "\n";
        }
        return static_cast<int>(dibyt::cli::ExitCode::kSuccess);
    }

    if (opts.command == "status") {
        if (opts.json) {
            dibyt::cli::print_json_status("idle", "No active transfer running");
        } else {
            std::cout << "Dibyt Daemon Status: IDLE (No transfers running)\n";
        }
        return static_cast<int>(dibyt::cli::ExitCode::kSuccess);
    }

    return static_cast<int>(dibyt::cli::ExitCode::kSuccess);
}
