#include <cassert>
#include <iostream>
#include <filesystem>
#include <span>
#include <vector>
#include "dibyt/transfer/checkpoint.hpp"

// Test suite for durable checkpointing, crash truncation, and reconnect reconciliation
int main() {
    std::cout << "[TEST] Running Durable Checkpoint & Resume Engine Tests...\n";

    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "dibyt_checkpoint_test";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    std::filesystem::path db_path = temp_dir / "meta.db";
    dibyt::storage::Database db;
    assert(db.open(db_path) == dibyt::core::ErrorCode::kOk);

    // Seed task
    dibyt::storage::TaskRecord task;
    task.id = dibyt::core::TaskId("task_resumable_01");
    task.session_id = dibyt::core::SessionId("sess_01");
    task.file_id = dibyt::core::FileId("f_01");
    task.relative_path = "large_model.bin";
    task.total_size = 5000;
    task.durable_offset = 0;
    assert(db.insert_task(task) == dibyt::core::ErrorCode::kOk);

    // 1. Initial write with checkpoint interval = 1000 bytes
    {
        dibyt::transfer::CheckpointStream stream(db);
        assert(stream.open_or_resume(temp_dir, task.id, 5000, 1000) == dibyt::core::ErrorCode::kOk);
        assert(stream.durable_offset() == 0);

        // Write chunk 1: 500 bytes (no checkpoint commit yet)
        std::vector<uint8_t> chunk1(500, 0xAA);
        bool committed = false;
        assert(stream.write_chunk(0, chunk1, committed) == dibyt::core::ErrorCode::kOk);
        assert(!committed);
        assert(stream.durable_offset() == 0);

        // Write chunk 2: 600 bytes (total 1100 >= 1000 interval -> commits at 1100)
        std::vector<uint8_t> chunk2(600, 0xBB);
        assert(stream.write_chunk(500, chunk2, committed) == dibyt::core::ErrorCode::kOk);
        assert(committed);
        assert(stream.durable_offset() == 1100);

        // Write un-checkpointed dirty bytes: 400 bytes (simulating write before crash)
        std::vector<uint8_t> dirty_chunk(400, 0xCC);
        assert(stream.write_chunk(1100, dirty_chunk, committed) == dibyt::core::ErrorCode::kOk);
        assert(!committed);
        // Process crash simulation: destructor closes stream without committing dirty bytes
    }

    // 2. Crash Recovery & Resumption Validation
    // Database reflects last durable commit (1100). On re-opening, staging file must truncate dirty bytes back to 1100.
    {
        dibyt::storage::TaskRecord current_task;
        assert(db.get_task(task.id, current_task) == dibyt::core::ErrorCode::kOk);
        assert(current_task.durable_offset == 1100);

        dibyt::transfer::CheckpointStream resume_stream(db);
        assert(resume_stream.open_or_resume(temp_dir, task.id, 5000, 1000) == dibyt::core::ErrorCode::kOk);
        assert(resume_stream.durable_offset() == 1100);

        // Verify staging file size on disk is truncated exactly to durable offset 1100
        std::error_code ec;
        uint64_t disk_size = std::filesystem::file_size(resume_stream.staging_path(), ec);
        assert(disk_size == 1100);

        // Attempting to write with gap must trigger protocol violation
        std::vector<uint8_t> invalid_gap(100, 0xDD);
        bool committed = false;
        assert(resume_stream.write_chunk(1500, invalid_gap, committed) == dibyt::core::ErrorCode::kProtocolViolation);

        // Contiguous resumption from 1100
        std::vector<uint8_t> resume_chunk(3900, 0xEE); // Completes the 5000 byte total
        assert(resume_stream.write_chunk(1100, resume_chunk, committed) == dibyt::core::ErrorCode::kOk);
        assert(committed);
        assert(resume_stream.durable_offset() == 5000);
        assert(resume_stream.finalize_transfer() == dibyt::core::ErrorCode::kOk);
    }

    // 3. Reconnection Reconciliation Test
    {
        dibyt::storage::TaskRecord final_task;
        assert(db.get_task(task.id, final_task) == dibyt::core::ErrorCode::kOk);
        assert(final_task.status == dibyt::storage::TaskStatus::kCompleted);

        dibyt::transfer::ReconcileRequest req;
        req.session_id = dibyt::core::SessionId("sess_01");
        req.task_id = dibyt::core::TaskId("task_resumable_01");
        req.file_id = dibyt::core::FileId("f_01");
        req.file_size = 5000;
        req.receiver_claimed_offset = 1100;

        dibyt::transfer::ReconcileResult result;
        assert(dibyt::transfer::reconcile_reconnect(req, final_task, result) == dibyt::core::ErrorCode::kOk);
        assert(result.can_resume);
        assert(result.resume_offset == 1100);
    }

    db.close();
    std::filesystem::remove_all(temp_dir);

    std::cout << "[SMOKE_TEST_PASSED] Durable checkpointing and reconnect reconciliation verified.\n";
    return 0;
}
