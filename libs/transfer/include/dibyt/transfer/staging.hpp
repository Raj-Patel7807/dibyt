#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <span>
#include <vector>
#include <fstream>
#include <cstdint>
#include "dibyt/core/error.hpp"
#include "dibyt/core/types.hpp"

namespace dibyt::transfer {

enum class OverwritePolicy {
    kDisallow,      // Reject and abort if target file already exists (default secure)
    kOverwrite      // Atomically replace existing destination file
};

// Destination staging and atomic commit manager
class StagedFileWriter {
public:
    StagedFileWriter() = default;
    ~StagedFileWriter() { abort(); }

    StagedFileWriter(const StagedFileWriter&) = delete;
    StagedFileWriter& operator=(const StagedFileWriter&) = delete;

    StagedFileWriter(StagedFileWriter&&) noexcept;
    StagedFileWriter& operator=(StagedFileWriter&&) noexcept;

    // Initialize temporary staging file in staging directory on same filesystem root
    [[nodiscard]] core::ErrorCode open(
        const std::filesystem::path& destination_root,
        std::string_view relative_target_path,
        std::string_view expected_sha256_hex,
        uint64_t expected_total_size,
        OverwritePolicy policy = OverwritePolicy::kDisallow
    );

    // Stream incoming payload chunks into the staging file
    [[nodiscard]] core::ErrorCode append_chunk(std::span<const uint8_t> chunk);

    // Flush and synchronize dirty OS page cache to physical storage
    [[nodiscard]] core::ErrorCode flush_to_disk();

    // Verify cryptographic SHA-256 and atomically rename into target path
    [[nodiscard]] core::ErrorCode finalize();

    // Purge staging file if aborted or verification failed
    void abort() noexcept;

    [[nodiscard]] uint64_t bytes_written() const noexcept { return bytes_written_; }
    [[nodiscard]] const std::filesystem::path& staging_path() const noexcept { return staging_path_; }
    [[nodiscard]] const std::filesystem::path& final_path() const noexcept { return final_path_; }

private:
    std::filesystem::path staging_path_;
    std::filesystem::path final_path_;
    std::string expected_sha256_hex_;
    uint64_t expected_total_size_{0};
    uint64_t bytes_written_{0};
    OverwritePolicy policy_{OverwritePolicy::kDisallow};
    std::ofstream file_stream_;
    bool is_finalized_{false};
};

} // namespace dibyt::transfer
