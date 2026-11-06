#include "dibyt/transfer/checkpoint.hpp"
#include <iostream>

namespace dibyt::transfer {

CheckpointStream::CheckpointStream(storage::Database& db) : db_(db) {}

CheckpointStream::~CheckpointStream() {
    if (stream_.is_open()) {
        stream_.close();
    }
}

core::ErrorCode CheckpointStream::open_or_resume(
    const std::filesystem::path& destination_root,
    const core::TaskId& task_id,
    uint64_t total_expected_size,
    uint64_t checkpoint_interval_bytes
) {
    task_id_ = task_id;
    total_expected_size_ = total_expected_size;
    checkpoint_interval_bytes_ = checkpoint_interval_bytes;
    uncommitted_bytes_ = 0;

    // 1. Query SQLite for authoritative durable offset
    storage::TaskRecord record;
    auto err = db_.get_task(task_id_, record);
    if (err != core::ErrorCode::kOk) {
        return err;
    }

    durable_offset_ = record.durable_offset;

    // 2. Prepare staging file
    std::filesystem::path staging_dir = destination_root / ".dibyt-staging";
    std::error_code ec;
    std::filesystem::create_directories(staging_dir, ec);
    if (ec) {
        return core::ErrorCode::kStorageError;
    }

    staging_path_ = staging_dir / ("task_" + task_id_.value() + ".part");

    // 3. Crash recovery reconciliation on disk: truncate staging file back to SQLite durable offset
    if (std::filesystem::exists(staging_path_, ec)) {
        uint64_t on_disk_size = std::filesystem::file_size(staging_path_, ec);
        if (!ec && on_disk_size > durable_offset_) {
            // Un-checkpointed dirty bytes exist due to prior crash; truncate to durable offset
            std::filesystem::resize_file(staging_path_, durable_offset_, ec);
            if (ec) {
                return core::ErrorCode::kStorageError;
            }
        }
    }

    // 4. Open in read/write binary mode
    stream_.open(staging_path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!stream_.is_open()) {
        // If file didn't exist yet, create it
        stream_.clear();
        stream_.open(staging_path_, std::ios::out | std::ios::binary);
        stream_.close();
        stream_.open(staging_path_, std::ios::in | std::ios::out | std::ios::binary);
        if (!stream_.is_open()) {
            return core::ErrorCode::kStorageError;
        }
    }

    // Seek to durable offset
    stream_.seekp(static_cast<std::streamoff>(durable_offset_));
    return core::ErrorCode::kOk;
}

core::ErrorCode CheckpointStream::write_chunk(
    uint64_t offset, 
    std::span<const uint8_t> data,
    bool& out_checkpoint_committed
) {
    out_checkpoint_committed = false;

    if (!stream_.is_open()) {
        return core::ErrorCode::kStorageError;
    }

    // Strict contiguous sequencing enforcement: disallow gap or out-of-order writes
    if (offset != durable_offset_ + uncommitted_bytes_) {
        return core::ErrorCode::kProtocolViolation;
    }

    if (data.empty()) {
        return core::ErrorCode::kOk;
    }

    stream_.write(
        reinterpret_cast<const char*>(data.data()), 
        static_cast<std::streamsize>(data.size())
    );

    if (stream_.bad()) {
        return core::ErrorCode::kStorageError;
    }

    uncommitted_bytes_ += data.size();

    // Check if checkpoint interval has elapsed or end-of-file reached
    uint64_t total_written = durable_offset_ + uncommitted_bytes_;
    if (uncommitted_bytes_ >= checkpoint_interval_bytes_ || total_written >= total_expected_size_) {
        auto err = commit_checkpoint_internal(total_written);
        if (err != core::ErrorCode::kOk) {
            return err;
        }
        out_checkpoint_committed = true;
    }

    return core::ErrorCode::kOk;
}

core::ErrorCode CheckpointStream::commit_checkpoint_internal(uint64_t offset) {
    // 1. Flush OS buffer to disk
    stream_.flush();
    if (stream_.bad()) {
        return core::ErrorCode::kStorageError;
    }

    // 2. Commit transactionally to SQLite
    storage::Transaction txn(db_);
    auto err = db_.update_checkpoint(task_id_, offset);
    if (err != core::ErrorCode::kOk) {
        return err;
    }
    err = txn.commit();
    if (err != core::ErrorCode::kOk) {
        return err;
    }

    // 3. Advance internal durable offset
    durable_offset_ = offset;
    uncommitted_bytes_ = 0;
    return core::ErrorCode::kOk;
}

core::ErrorCode CheckpointStream::finalize_transfer() {
    if (durable_offset_ < total_expected_size_) {
        return core::ErrorCode::kIntegrityMismatch;
    }
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
    return core::ErrorCode::kOk;
}

core::ErrorCode reconcile_reconnect(
    const ReconcileRequest& req,
    const storage::TaskRecord& current_task,
    ReconcileResult& out_result
) {
    out_result.can_resume = false;
    out_result.resume_offset = 0;

    // Validate identity matching
    if (req.task_id != current_task.id || req.session_id != current_task.session_id) {
        return core::ErrorCode::kInvalidArgument;
    }

    // Validate size and bounds
    if (req.file_size != current_task.total_size) {
        return core::ErrorCode::kSourceMutated;
    }

    // Invariant: Authoritative resume offset is bounded by the receiver's durable progress
    if (req.receiver_claimed_offset > current_task.total_size) {
        return core::ErrorCode::kProtocolViolation;
    }

    out_result.can_resume = true;
    out_result.resume_offset = req.receiver_claimed_offset;
    return core::ErrorCode::kOk;
}

} // namespace dibyt::transfer
