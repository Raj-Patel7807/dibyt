#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include "dibyt/manifest/manifest.hpp"

// Test suite for generic manifest model and deterministic scanning
int main() {
    std::cout << "[TEST] Running Manifest and Path Normalization Tests...\n";

    // 1. Path normalization and traversal rejection
    {
        std::string norm;
        assert(dibyt::manifest::sanitize_relative_path("foo/bar.txt", norm) == dibyt::core::ErrorCode::kOk);
        assert(norm == "foo/bar.txt");

        assert(dibyt::manifest::sanitize_relative_path("a/b/../c", norm) == dibyt::core::ErrorCode::kSecurityViolation);
        assert(dibyt::manifest::sanitize_relative_path("../../secret.key", norm) == dibyt::core::ErrorCode::kSecurityViolation);
    }

    // 2. Setup temporary mock directory tree
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "dibyt_manifest_test";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir / "subdir");
    std::filesystem::create_directories(temp_dir / "nested" / "deep");

    // Create test files
    {
        std::ofstream f1(temp_dir / "root.bin", std::ios::binary);
        f1 << "hello dibyt";
        std::ofstream f2(temp_dir / "subdir" / "data.txt");
        f2 << "sub directory contents";
        std::ofstream f3(temp_dir / "nested" / "deep" / "leaf.dat");
        f3 << "deep nested data";
    }

    // 3. Scan directory and generate manifest
    dibyt::manifest::ScannerOptions options;
    dibyt::manifest::Manifest manifest;
    auto err = dibyt::manifest::build_collection_manifest(temp_dir, "test_collection", options, manifest);
    assert(err == dibyt::core::ErrorCode::kOk);
    assert(manifest.collection_name == "test_collection");
    assert(manifest.files.size() == 3);

    // Verify deterministic alphabetical ordering
    assert(manifest.files[0].relative_path == "nested/deep/leaf.dat");
    assert(manifest.files[1].relative_path == "root.bin");
    assert(manifest.files[2].relative_path == "subdir/data.txt");

    // Verify SHA-256 digests are non-empty and 64 characters
    for (const auto& f : manifest.files) {
        assert(f.sha256_hex.length() == 64);
        assert(!f.id.empty());
    }

    // 4. Test source mutation detection
    assert(dibyt::manifest::verify_source_integrity(temp_dir, manifest) == dibyt::core::ErrorCode::kOk);

    // Mutate one file
    {
        std::ofstream f1(temp_dir / "root.bin", std::ios::app);
        f1 << "tampered";
    }
    assert(dibyt::manifest::verify_source_integrity(temp_dir, manifest) == dibyt::core::ErrorCode::kSourceMutated);

    // Cleanup
    std::filesystem::remove_all(temp_dir);

    std::cout << "[SMOKE_TEST_PASSED] Manifest scanning, ordering, and SHA-256 verified successfully.\n";
    return 0;
}
