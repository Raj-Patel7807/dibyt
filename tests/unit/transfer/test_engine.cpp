#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include "dibyt/transfer/engine.hpp"
#include "dibyt/manifest/manifest.hpp"

// End-to-end integration test validating multi-worker parallel transfer of a nested collection
int main() {
    std::cout << "[TEST] Running End-to-End Parallel TCP Transfer Tests...\n";

    std::filesystem::path root_test_dir = std::filesystem::temp_directory_path() / "dibyt_e2e_transfer_test";
    std::filesystem::remove_all(root_test_dir);

    std::filesystem::path src_dir = root_test_dir / "src";
    std::filesystem::path dst_dir = root_test_dir / "dst";
    std::filesystem::path db_path = root_test_dir / "meta.db";

    std::filesystem::create_directories(src_dir / "deep" / "nested");
    std::filesystem::create_directories(src_dir / "assets");

    // 1. Create a diverse set of test files
    {
        std::ofstream f1(src_dir / "file_small.txt");
        f1 << "small content 123";

        std::ofstream f2(src_dir / "deep" / "nested" / "model.bin", std::ios::binary);
        std::vector<uint8_t> large_payload(250000, 0xFE); // 250 KiB
        f2.write(reinterpret_cast<const char*>(large_payload.data()), static_cast<std::streamsize>(large_payload.size()));

        std::ofstream f3(src_dir / "assets" / "image.png", std::ios::binary);
        std::vector<uint8_t> img_payload(120000, 0x42);
        f3.write(reinterpret_cast<const char*>(img_payload.data()), static_cast<std::streamsize>(img_payload.size()));
    }

    // 2. Scan source directory to generate collection manifest
    dibyt::manifest::ScannerOptions scan_opts;
    dibyt::manifest::Manifest manifest;
    assert(dibyt::manifest::build_collection_manifest(src_dir, "test_col", scan_opts, manifest) == dibyt::core::ErrorCode::kOk);
    assert(manifest.files.size() == 3);

    // 3. Initialize Transfer Engine with 4 parallel workers
    dibyt::storage::Database db;
    assert(db.open(db_path) == dibyt::core::ErrorCode::kOk);

    dibyt::transfer::EngineConfig config;
    config.num_workers = 4;
    config.chunk_size_bytes = 16384; // 16 KiB chunks to test multiple stream iterations
    dibyt::transfer::TransferEngine engine(db, config);

    // 4. Execute parallel transfer
    auto err = engine.execute_local_parallel_transfer(src_dir, dst_dir, manifest);
    assert(err == dibyt::core::ErrorCode::kOk);

    // 5. Verify all destination files exist and match source SHA-256
    for (const auto& file_meta : manifest.files) {
        std::filesystem::path dest_file = dst_dir / std::filesystem::path(file_meta.relative_path);
        assert(std::filesystem::exists(dest_file));
        assert(std::filesystem::file_size(dest_file) == file_meta.size_bytes);

        std::string computed_hash;
        assert(dibyt::manifest::compute_file_sha256(dest_file, computed_hash) == dibyt::core::ErrorCode::kOk);
        assert(computed_hash == file_meta.sha256_hex);
    }

    // 6. Verify quarantine directory is empty/cleaned up
    assert(!std::filesystem::exists(dst_dir / ".dibyt-staging") || 
           std::filesystem::is_empty(dst_dir / ".dibyt-staging"));

    db.close();
    std::filesystem::remove_all(root_test_dir);

    std::cout << "[SMOKE_TEST_PASSED] End-to-end parallel transfer of nested collection verified.\n";
    return 0;
}
