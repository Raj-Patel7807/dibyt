#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include "dibyt/scheduler/scheduler.hpp"

// Test suite for dynamic task scheduler, lease fencing, and worker pool fairness
int main() {
    std::cout << "[TEST] Running Dynamic Task Scheduler & Lease Fencing Tests...\n";

    std::filesystem::path temp_db = std::filesystem::temp_directory_path() / "dibyt_sched_test.db";
    std::filesystem::remove(temp_db);

    dibyt::storage::Database db;
    assert(db.open(temp_db) == dibyt::core::ErrorCode::kOk);

    dibyt::scheduler::SchedulerConfig config;
    config.default_lease_ttl_sec = 2; // Short 2-second lease for testing
    config.max_queue_depth = 100;

    dibyt::scheduler::TaskScheduler scheduler(db, config);

    // 1. Enqueue 3 tasks of uneven sizes: 10KB, 500MB, 1MB
    std::vector<dibyt::storage::TaskRecord> input_tasks;
    {
        dibyt::storage::TaskRecord t1;
        t1.id = dibyt::core::TaskId("task_small");
        t1.total_size = 10240; // 10 KiB
        input_tasks.push_back(t1);

        dibyt::storage::TaskRecord t2;
        t2.id = dibyt::core::TaskId("task_huge");
        t2.total_size = 524288000; // 500 MiB
        input_tasks.push_back(t2);

        dibyt::storage::TaskRecord t3;
        t3.id = dibyt::core::TaskId("task_med");
        t3.total_size = 1048576; // 1 MiB
        input_tasks.push_back(t3);
    }

    assert(scheduler.enqueue_session_tasks(dibyt::core::SessionId("sess_01"), input_tasks) == dibyt::core::ErrorCode::kOk);
    assert(scheduler.pending_count() == 3);

    // 2. Worker 1 pulls work: must receive the LARGEST task first (LPT scheduling)
    std::optional<dibyt::scheduler::LeaseToken> lease_w1;
    dibyt::scheduler::ScheduledTask task_w1;
    assert(scheduler.acquire_lease("worker_01", lease_w1, task_w1) == dibyt::core::ErrorCode::kOk);
    assert(lease_w1.has_value());
    assert(task_w1.id == dibyt::core::TaskId("task_huge"));
    assert(task_w1.total_size_bytes == 524288000);
    assert(scheduler.pending_count() == 2);
    assert(scheduler.active_lease_count() == 1);

    // 3. Worker 2 pulls work: receives 1MB task
    std::optional<dibyt::scheduler::LeaseToken> lease_w2;
    dibyt::scheduler::ScheduledTask task_w2;
    assert(scheduler.acquire_lease("worker_02", lease_w2, task_w2) == dibyt::core::ErrorCode::kOk);
    assert(lease_w2.has_value());
    assert(task_w2.id == dibyt::core::TaskId("task_med"));

    // 4. Stale Worker Fencing Defense
    // Simulate Worker 1 and Worker 2 stalling until lease expires
    std::this_thread::sleep_for(std::chrono::milliseconds(2500)); // Sleep past 2s TTL

    // Housekeeping reclaims expired lease
    uint32_t reclaimed = scheduler.reclaim_expired_leases();
    assert(reclaimed >= 1); // Expired leased tasks reclaimed

    // Worker 3 now claims the huge task
    std::optional<dibyt::scheduler::LeaseToken> lease_w3;
    dibyt::scheduler::ScheduledTask task_w3;
    assert(scheduler.acquire_lease("worker_03", lease_w3, task_w3) == dibyt::core::ErrorCode::kOk);
    assert(lease_w3.has_value());
    assert(task_w3.id == dibyt::core::TaskId("task_huge"));
    // New fencing generation assigned
    assert(lease_w3->fence_generation > lease_w1->fence_generation);

    // Zombie Worker 1 attempts to complete task with OLD fence token: MUST BE FENCED OUT
    auto err_stale = scheduler.complete_task(*lease_w1);
    assert(err_stale == dibyt::core::ErrorCode::kTaskLeaseExpired);

    // Valid Worker 3 completes task with CURRENT fence token: SUCCEEDS
    assert(scheduler.complete_task(*lease_w3) == dibyt::core::ErrorCode::kOk);
    assert(scheduler.completed_count() == 1);

    db.close();
    std::filesystem::remove(temp_db);

    std::cout << "[SMOKE_TEST_PASSED] Dynamic task scheduler, LPT ordering, and lease fencing verified.\n";
    return 0;
}
