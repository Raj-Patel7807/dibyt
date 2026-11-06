#include "dibyt/transfer/staging.hpp"
#include "dibyt/manifest/manifest.hpp"
#include <chrono>
#include <random>
#include <sstream>

#if !defined(_WIN32)
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace dibyt::transfer {

namespace {

std::string generate_random_tag() {
    static thread_local std::mt19937_64 rng(
        static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );
    uint64_t r = rng();
    std::ostringstream oss;
    oss << std::hex << r;
    return oss.str();
}

} // namespace

StagedFileWriter::StagedFileWriter(StagedFileWriter&& other) noexcept
    : staging_path_(std::move(other.staging_path_)),
      final_path_(std::move(other.final_path_)),
      expected_sha256_hex_(std::move(other.expected_sha256_hex_)),
      expected_total_size_(other.expected_total_size_),
      bytes_written_(other.bytes_written_),
      policy_(other.policy_),
      file_stream_(std::move(other.file_stream_)),
      is_finalized_(other.is_finalized_) {
    other.is_finalized_ = true; // Prevent destructor of moved-from instance from unlinking
}

StagedFileWriter& StagedFileWriter::operator=(StagedFileWriter&& other) noexcept {
    if (this != &other) {
        abort();
        staging_path_ = std::move(other.staging_path_);
        final_path_ = std::move(other.final_path_);
        expected_sha256_hex_ = std::move(other.expected_sha256_hex_);
        expected_total_size_ = other.expected_total_size_;
        bytes_written_ = other.bytes_written_;
        policy_ = other.policy_;
        file_stream_ = std::move(other.file_stream_);
        is_finalized_ = other.is_finalized_;
        other.is_finalized_ = true;
    }
    return *this;
}

core::ErrorCode StagedFileWriter::open(
    const std::filesystem::path& destination_root,
    std::string_view relative_target_path,
    std::string_view expected_sha256_hex,
    uint64_t expected_total_size,
    OverwritePolicy policy
) {
    abort();
    policy_ = policy;
    expected_sha256_hex_ = expected_sha256_hex;
    expected_total_size_ = expected_total_size;
    bytes_written_ = 0;
    is_finalized_ = false;

    // Validate relative destination path
    std::string sanitized_rel;
    auto err = manifest::sanitize_relative_path(std::filesystem::path(relative_target_path), sanitized_rel);
    if (err != core::ErrorCode::kOk) {
        return err;
    }

    std::error_code ec;
    final_path_ = destination_root / std::filesystem::path(sanitized_rel);

    // Collision check: default policy strictly forbids silent overwrite
    if (std::filesystem::exists(final_path_, ec) && policy_ == OverwritePolicy::kDisallow) {
        return core::ErrorCode::kStorageError;
    }

    // Ensure staging directory exists within the SAME destination root to guarantee atomic rename
    std::filesystem::path staging_dir = destination_root / ".dibyt-staging";
    std::filesystem::create_directories(staging_dir, ec);
    if (ec) {
        return core::ErrorCode::kStorageError;
    }

    // Construct unique staging file name
    std::string tag = generate_random_tag();
    staging_path_ = staging_dir / ("dibyt_part_" + tag + ".tmp");

    file_stream_.open(staging_path_, std::ios::binary | std::ios::trunc | std::ios::out);
    if (!file_stream_.is_open()) {
        return core::ErrorCode::kStorageError;
    }

    return core::ErrorCode::kOk;
}

core::ErrorCode StagedFileWriter::append_chunk(std::span<const uint8_t> chunk) {
    if (!file_stream_.is_open() || is_finalized_) {
        return core::ErrorCode::kStorageError;
    }

    if (chunk.empty()) {
        return core::ErrorCode::kOk;
    }

    file_stream_.write(
        reinterpret_cast<const char*>(chunk.data()), 
        static_cast<std::streamsize>(chunk.size())
    );

    if (file_stream_.bad()) {
        return core::ErrorCode::kStorageError;
    }

    bytes_written_ += chunk.size();
    return core::ErrorCode::kOk;
}

core::ErrorCode StagedFileWriter::flush_to_disk() {
    if (!file_stream_.is_open()) {
        return core::ErrorCode::kStorageError;
    }
    file_stream_.flush();
    if (file_stream_.bad()) {
        return core::ErrorCode::kStorageError;
    }
    return core::ErrorCode::kOk;
}

core::ErrorCode StagedFileWriter::finalize() {
    if (!file_stream_.is_open() || is_finalized_) {
        return core::ErrorCode::kStorageError;
    }

    // 1. Flush and close staging stream
    file_stream_.flush();
    file_stream_.close();

    // 2. Validate total byte count against manifest
    if (bytes_written_ != expected_total_size_) {
        abort();
        return core::ErrorCode::kIntegrityMismatch;
    }

    // 3. Compute complete SHA-256 over staging file
    std::string computed_sha256;
    auto err = manifest::compute_file_sha256(staging_path_, computed_sha256);
    if (err != core::ErrorCode::kOk) {
        abort();
        return err;
    }

    // 4. Verify digest matches expected manifest hash
    if (computed_sha256 != expected_sha256_hex_) {
        abort();
        return core::ErrorCode::kIntegrityMismatch;
    }

    // 5. Create final destination parent directories if needed
    std::error_code ec;
    std::filesystem::create_directories(final_path_.parent_path(), ec);
    if (ec) {
        abort();
        return core::ErrorCode::kStorageError;
    }

    // 6. Atomic filesystem rename
    if (policy_ == OverwritePolicy::kOverwrite && std::filesystem::exists(final_path_, ec)) {
        std::filesystem::remove(final_path_, ec);
    }

    std::filesystem::rename(staging_path_, final_path_, ec);
    if (ec) {
        abort();
        return core::ErrorCode::kStorageError;
    }

    is_finalized_ = true;
    return core::ErrorCode::kOk;
}

void StagedFileWriter::abort() noexcept {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    if (!is_finalized_ && !staging_path_.empty()) {
        std::error_code ec;
        std::filesystem::remove(staging_path_, ec);
    }
    is_finalized_ = true;
}

} // namespace dibyt::transfer
