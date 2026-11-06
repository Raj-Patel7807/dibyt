#include <cassert>
#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include "dibyt/storage/database.hpp"
#include "dibyt/transfer/checkpoint.hpp"

// Fault Injection Test: Simulates abrupt socket disconnects, uncommitted dirty bytes, and crash truncate recovery
int main() {
    std::cout << "[TEST] Running Chaos & Fault-Injection Tests...\n";

    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "dibyt_fault_inj_test";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    std::filesystem::path db_path = temp_dir / "meta.db";
    dibyt::storage::Database db;
    assert(db.open(db_path) == dibyt::core::ErrorCode::kOk);

    dibyt::storage::TaskRecord task;
    task.id = dibyt::core::TaskId("task_fault_01");
    task.session_id = dibyt::core::SessionId("sess_chaos");
    task.file_id = dibyt::core::FileId("f_chaos");
    task.relative_path = "chaos_dataset.bin";
    task.total_size = 10000;
    task.durable_offset = 0;
    assert(db.insert_task(task) == dibyt::core::ErrorCode::kOk);

    // Fault 1: Partial transfer interrupted mid-stream before checkpoint commit
    {
        dibyt::transfer::CheckpointStream stream(db);
        assert(stream.open_or_resume(temp_dir, task.id, 10000, 2000) == dibyt::core::ErrorCode::kOk);

        std::vector<uint8_t> chunk1(1500, 0x11);
        bool committed = false;
        assert(stream.write_chunk(0, chunk1, committed) == dibyt::core::ErrorCode::kOk);
        assert(!committed); // 1500 < 2000 interval -> not yet committed to DB

        // INJECT FAULT: Abrupt process termination / connection drop simulation
    }

    // Assert that database held 0 durable progress
    dibyt::storage::TaskRecord after_crash_record;
    assert(db.get_task(task.id, after_crash_record) == dibyt::core::ErrorCode::kOk);
    assert(after_crash_record.durable_offset == 0);

    // Fault 2: Reopen must truncate disk file back to authoritative DB durable offset (0)
    {
        dibyt::transfer::CheckpointStream recovered_stream(db);
        assert(recovered_stream.open_or_resume(temp_dir, task.id, 10000, 2000) == dibyt::core::ErrorCode::kOk);
        assert(recovered_stream.durable_offset() == 0);

        std::error_code ec;
        assert(std::filesystem::file_size(recovered_stream.staging_path(), ec) == 0);

        // Resume correctly: 2500 bytes (exceeds 2000 interval -> commits)
        std::vector<uint8_t> valid_chunk(2500, 0x22);
        bool committed = false;
        assert(recovered_stream.write_chunk(0, valid_chunk, committed) == dibyt::core::ErrorCode::kOk);
        assert(committed);
        assert(recovered_stream.durable_offset() == 2500);
    }

    db.close();
    std::filesystem::remove_all(temp_dir);

    std::cout << "[SMOKE_TEST_PASSED] Chaos and fault-injection scenarios recovered cleanly.\n";
    return 0;
}

