#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <optional>
#include "dibyt/core/types.hpp"
#include "dibyt/core/error.hpp"

namespace dibyt::manifest {

enum class SymlinkPolicy {
    kDisallow,      // Reject symlinks (default secure setting)
    kFollowInternal // Allow symlinks only if target stays within collection root
};

struct FileEntry {
    core::FileId id;
    std::string relative_path;   // Normalized POSIX-style relative path (e.g. "sub/file.bin")
    uint64_t size_bytes{0};
    uint64_t modified_epoch_sec{0};
    std::string sha256_hex;      // Lowercase 64-char hex string (empty if unhashed)

    auto operator<=>(const FileEntry& other) const noexcept {
        return relative_path <=> other.relative_path;
    }
};

struct Manifest {
    core::CollectionId collection_id;
    std::string collection_name;
    uint32_t version{1};
    uint64_t created_epoch_sec{0};
    uint64_t total_size_bytes{0};
    std::vector<FileEntry> files;
};

struct ScannerOptions {
    SymlinkPolicy symlink_policy{SymlinkPolicy::kDisallow};
    bool include_hidden{false};
    uint32_t max_file_count{100000};
    uint64_t max_total_size_bytes{10ULL * 1024ULL * 1024ULL * 1024ULL}; // 10 GiB guard
    size_t max_path_length{1024};
};

// Canonical path normalization and safety checks
[[nodiscard]] core::ErrorCode sanitize_relative_path(
    const std::filesystem::path& path, 
    std::string& out_normalized_path
);

// High-performance SHA-256 calculator (OpenSSL EVP or standard fallback)
[[nodiscard]] core::ErrorCode compute_file_sha256(
    const std::filesystem::path& absolute_path, 
    std::string& out_hex
);

// Generate an immutable collection manifest by scanning a directory root
[[nodiscard]] core::ErrorCode build_collection_manifest(
    const std::filesystem::path& root_path,
    const std::string& collection_name,
    const ScannerOptions& options,
    Manifest& out_manifest
);

// Verify if a source directory has mutated relative to an existing manifest
[[nodiscard]] core::ErrorCode verify_source_integrity(
    const std::filesystem::path& root_path,
    const Manifest& manifest
);

} // namespace dibyt::manifest
