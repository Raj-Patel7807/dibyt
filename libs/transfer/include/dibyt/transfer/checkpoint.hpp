#pragma once

#include <string>
#include <cstdint>
#include <filesystem>
#include <span>
#include <fstream>
#include "dibyt/core/error.hpp"
#include "dibyt/core/types.hpp"
#include "dibyt/storage/database.hpp"

namespace dibyt::transfer {

// Resumable receiver stream tracking durable disk offsets and SQLite state
class CheckpointStream {
public:
    CheckpointStream(storage::Database& db);
    ~CheckpointStream();

    CheckpointStream(const CheckpointStream&) = delete;
    CheckpointStream& operator=(const CheckpointStream&) = delete;

    // Open or resume a task staging file, truncating any dirty un-checkpointed bytes
    [[nodiscard]] core::ErrorCode open_or_resume(
        const std::filesystem::path& destination_root,
        const core::TaskId& task_id,
        uint64_t total_expected_size,
        uint64_t checkpoint_interval_bytes = 1048576
    );

    // Write chunk at expected offset; enforces contiguous sequencing
    [[nodiscard]] core::ErrorCode write_chunk(
        uint64_t offset, 
        std::span<const uint8_t> data,
        bool& out_checkpoint_committed
    );

    // Finalize: verify complete receipt, sync, and update SQLite state to COMPLETED
    [[nodiscard]] core::ErrorCode finalize_transfer();

    [[nodiscard]] uint64_t durable_offset() const noexcept { return durable_offset_; }
    [[nodiscard]] const std::filesystem::path& staging_path() const noexcept { return staging_path_; }

private:
    [[nodiscard]] core::ErrorCode commit_checkpoint_internal(uint64_t offset);

    storage::Database& db_;
    core::TaskId task_id_;
    std::filesystem::path staging_path_;
    uint64_t total_expected_size_{0};
    uint64_t durable_offset_{0};
    uint64_t uncommitted_bytes_{0};
    uint64_t checkpoint_interval_bytes_{1048576};
    std::fstream stream_;
};

// Reconnection reconciliation engine
struct ReconcileRequest {
    core::SessionId session_id;
    core::TaskId task_id;
    core::FileId file_id;
    std::string expected_sha256;
    uint64_t file_size{0};
    uint64_t receiver_claimed_offset{0};
};

struct ReconcileResult {
    bool can_resume{false};
    uint64_t resume_offset{0};
};

// Validates receiver resumption claims against authoritative sender manifest and SQLite bounds
[[nodiscard]] core::ErrorCode reconcile_reconnect(
    const ReconcileRequest& req,
    const storage::TaskRecord& current_task,
    ReconcileResult& out_result
);

} // namespace dibyt::transfer
