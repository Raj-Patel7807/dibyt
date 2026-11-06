#include <cassert>
#include <iostream>
#include <filesystem>
#include "dibyt/storage/database.hpp"

// Test suite for metadata storage, schema versioning, and transaction semantics
int main() {
    std::cout << "[TEST] Running SQLite Persistence & Schema Migration Tests...\n";

    std::filesystem::path test_db_path = std::filesystem::temp_directory_path() / "dibyt_test_metadata.db";
    std::filesystem::remove(test_db_path);

    dibyt::storage::Database db;
    auto err = db.open(test_db_path);
    assert(err == dibyt::core::ErrorCode::kOk);
    assert(db.is_open());
    assert(db.current_schema_version() == 1);

    // 1. Insert & Query Collection Record
    dibyt::storage::CollectionRecord col;
    col.id = dibyt::core::CollectionId("col_123");
    col.name = "prod_dataset";
    col.total_size_bytes = 104857600; // 100 MB
    col.file_count = 10;
    col.created_at = 1700000000;
    assert(db.insert_collection(col) == dibyt::core::ErrorCode::kOk);

    // Duplicate key rejection
    assert(db.insert_collection(col) == dibyt::core::ErrorCode::kStorageError);

    dibyt::storage::CollectionRecord retrieved_col;
    assert(db.get_collection(col.id, retrieved_col) == dibyt::core::ErrorCode::kOk);
    assert(retrieved_col.name == "prod_dataset");
    assert(retrieved_col.file_count == 10);

    // 2. Transaction RAII Rollback and Commit Semantics
    {
        dibyt::storage::Transaction txn(db);
        dibyt::storage::TaskRecord t1;
        t1.id = dibyt::core::TaskId("task_001");
        t1.session_id = dibyt::core::SessionId("sess_abc");
        t1.file_id = dibyt::core::FileId("f_001");
        t1.relative_path = "data/file1.bin";
        t1.total_size = 50000;
        t1.durable_offset = 0;
        assert(db.insert_task(t1) == dibyt::core::ErrorCode::kOk);
        assert(txn.commit() == dibyt::core::ErrorCode::kOk);
    }

    // 3. Checkpoint Updates & Bounds Checking
    assert(db.update_checkpoint(dibyt::core::TaskId("task_001"), 25000) == dibyt::core::ErrorCode::kOk);
    dibyt::storage::TaskRecord fetched_t;
    assert(db.get_task(dibyt::core::TaskId("task_001"), fetched_t) == dibyt::core::ErrorCode::kOk);
    assert(fetched_t.durable_offset == 25000);
    assert(fetched_t.status != dibyt::storage::TaskStatus::kCompleted);

    // Exceeding total size must fail
    assert(db.update_checkpoint(dibyt::core::TaskId("task_001"), 999999) == dibyt::core::ErrorCode::kInvalidArgument);

    // Complete checkpoint
    assert(db.update_checkpoint(dibyt::core::TaskId("task_001"), 50000) == dibyt::core::ErrorCode::kOk);
    assert(db.get_task(dibyt::core::TaskId("task_001"), fetched_t) == dibyt::core::ErrorCode::kOk);
    assert(fetched_t.status == dibyt::storage::TaskStatus::kCompleted);

    // 4. Dynamic Task Leasing and Heartbeat Expiration
    dibyt::storage::TaskRecord t2;
    t2.id = dibyt::core::TaskId("task_002");
    t2.session_id = dibyt::core::SessionId("sess_abc");
    t2.file_id = dibyt::core::FileId("f_002");
    t2.relative_path = "data/file2.bin";
    t2.total_size = 10000;
    assert(db.insert_task(t2) == dibyt::core::ErrorCode::kOk);

    std::optional<dibyt::storage::TaskRecord> leased_task;
    assert(db.lease_next_unassigned_task(dibyt::core::SessionId("sess_abc"), "worker_1", 10, leased_task) == dibyt::core::ErrorCode::kOk);
    assert(leased_task.has_value());
    assert(leased_task->id == t2.id);
    assert(leased_task->assigned_worker == "worker_1");

    // Second lease attempt when all tasks leased should yield nullopt
    std::optional<dibyt::storage::TaskRecord> empty_lease;
    assert(db.lease_next_unassigned_task(dibyt::core::SessionId("sess_abc"), "worker_2", 10, empty_lease) == dibyt::core::ErrorCode::kOk);
    assert(!empty_lease.has_value());

    db.close();
    std::filesystem::remove(test_db_path);

    std::cout << "[SMOKE_TEST_PASSED] Persistence and schema operations verified successfully.\n";
    return 0;
}
