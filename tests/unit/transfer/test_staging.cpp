#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <span>
#include "dibyt/transfer/staging.hpp"
#include "dibyt/manifest/manifest.hpp"

// Test suite for SHA-256 integrity verification and safe staging finalization
int main() {
    std::cout << "[TEST] Running SHA-256 Integrity & Safe Finalization Tests...\n";

    std::filesystem::path test_dest_root = std::filesystem::temp_directory_path() / "dibyt_staging_test";
    std::filesystem::remove_all(test_dest_root);
    std::filesystem::create_directories(test_dest_root);

    std::string test_data = "Production grade file payload verifying SHA-256 integrity.";
    std::span<const uint8_t> data_span(reinterpret_cast<const uint8_t*>(test_data.data()), test_data.size());

    // 1. Calculate true hash of the payload
    std::filesystem::path truth_file = test_dest_root / "truth.tmp";
    {
        std::ofstream out(truth_file, std::ios::binary);
        out << test_data;
    }
    std::string expected_sha256;
    assert(dibyt::manifest::compute_file_sha256(truth_file, expected_sha256) == dibyt::core::ErrorCode::kOk);
    std::filesystem::remove(truth_file);

    // 2. Successful Staged Transfer and Atomic Finalization
    {
        dibyt::transfer::StagedFileWriter writer;
        auto err = writer.open(test_dest_root, "models/weights.bin", expected_sha256, test_data.size());
        assert(err == dibyt::core::ErrorCode::kOk);

        // Stream chunks
        assert(writer.append_chunk(data_span) == dibyt::core::ErrorCode::kOk);
        assert(writer.flush_to_disk() == dibyt::core::ErrorCode::kOk);

        // Prior to finalize, final file must NOT exist
        assert(!std::filesystem::exists(test_dest_root / "models/weights.bin"));
        assert(std::filesystem::exists(writer.staging_path()));

        // Finalize
        assert(writer.finalize() == dibyt::core::ErrorCode::kOk);

        // After finalize, final file exists, staging file is cleaned up
        assert(std::filesystem::exists(test_dest_root / "models/weights.bin"));
        assert(!std::filesystem::exists(writer.staging_path()));
    }

    // 3. Collision Rejection: Default policy refuses silent overwrite
    {
        dibyt::transfer::StagedFileWriter collision_writer;
        auto err = collision_writer.open(
            test_dest_root, "models/weights.bin", expected_sha256, test_data.size(), 
            dibyt::transfer::OverwritePolicy::kDisallow
        );
        assert(err == dibyt::core::ErrorCode::kStorageError);
    }

    // 4. Integrity Mismatch Rejection & Auto-Quarantine Cleanup
    {
        dibyt::transfer::StagedFileWriter corrupt_writer;
        std::string forged_hash = "0000000000000000000000000000000000000000000000000000000000000000";
        auto err = corrupt_writer.open(test_dest_root, "corrupt/file.dat", forged_hash, test_data.size());
        assert(err == dibyt::core::ErrorCode::kOk);

        assert(corrupt_writer.append_chunk(data_span) == dibyt::core::ErrorCode::kOk);
        
        // Finalize must fail with integrity mismatch
        err = corrupt_writer.finalize();
        assert(err == dibyt::core::ErrorCode::kIntegrityMismatch);

        // Final destination path must NEVER be exposed
        assert(!std::filesystem::exists(test_dest_root / "corrupt/file.dat"));
        // Staged file must be aborted/unlinked
        assert(!std::filesystem::exists(corrupt_writer.staging_path()));
    }

    // Cleanup
    std::filesystem::remove_all(test_dest_root);

    std::cout << "[SMOKE_TEST_PASSED] SHA-256 verification and atomic finalization verified.\n";
    return 0;
}
