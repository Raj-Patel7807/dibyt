#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <mutex>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include "dibyt/core/error.hpp"
#include "dibyt/core/types.hpp"
#include "dibyt/storage/database.hpp"

namespace dibyt::scheduler {

// Explicit task lifecycle state machine
enum class LifecycleState : int {
    kPending = 0,
    kLeased = 1,
    kReceiving = 2,
    kVerifying = 3,
    kComplete = 4,
    kRetryableFailed = 5,
    kTerminalFailed = 6
};

[[nodiscard]] constexpr std::string_view lifecycle_state_to_string(LifecycleState state) noexcept {
    switch (state) {
        case LifecycleState::kPending: return "Pending";
        case LifecycleState::kLeased: return "Leased";
        case LifecycleState::kReceiving: return "Receiving";
        case LifecycleState::kVerifying: return "Verifying";
        case LifecycleState::kComplete: return "Complete";
        case LifecycleState::kRetryableFailed: return "RetryableFailed";
        case LifecycleState::kTerminalFailed: return "TerminalFailed";
    }
    return "Unknown";
}

// Fencing lease token preventing stale worker commits after lease expiry
struct LeaseToken {
    core::TaskId task_id;
    std::string worker_id;
    uint64_t lease_epoch_sec{0};
    uint64_t expires_epoch_sec{0};
    uint64_t fence_generation{0}; // Incremented on every lease grant to fence out stale workers
};

struct ScheduledTask {
    core::TaskId id;
    core::SessionId session_id;
    core::FileId file_id;
    std::string relative_path;
    uint64_t total_size_bytes{0};
    uint64_t durable_offset{0};
    LifecycleState state{LifecycleState::kPending};
    LeaseToken current_lease;
    uint32_t retry_count{0};
    uint32_t max_retries{5};
};

struct SchedulerConfig {
    uint32_t max_workers{32};
    uint32_t max_queue_depth{10000};
    uint64_t default_lease_ttl_sec{15};
};

// Dynamic thread-safe leased task scheduler with fairness and fencing
class TaskScheduler {
public:
    explicit TaskScheduler(storage::Database& db, SchedulerConfig config = {});
    ~TaskScheduler() = default;

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    // Enqueue tasks for a transfer session (sorted LPT - Longest Processing Time first)
    [[nodiscard]] core::ErrorCode enqueue_session_tasks(
        const core::SessionId& session_id,
        const std::vector<storage::TaskRecord>& tasks
    );

    // Worker lease acquisition: claims highest-priority available task
    [[nodiscard]] core::ErrorCode acquire_lease(
        std::string_view worker_id,
        std::optional<LeaseToken>& out_lease,
        ScheduledTask& out_task
    );

    // Heartbeat to extend active lease TTL
    [[nodiscard]] core::ErrorCode renew_lease(const LeaseToken& token);

    // Report task progression: state transition to Receiving / Verifying
    [[nodiscard]] core::ErrorCode transition_state(
        const LeaseToken& token, 
        LifecycleState target_state
    );

    // Complete task: strictly requires matching fence generation and verified finalization
    [[nodiscard]] core::ErrorCode complete_task(const LeaseToken& token);

    // Report failure: transitions to RetryableFailed (re-queued) or TerminalFailed
    [[nodiscard]] core::ErrorCode report_failure(const LeaseToken& token, bool fatal = false);

    // Housekeeping: reclaim expired leases whose workers timed out
    [[nodiscard]] uint32_t reclaim_expired_leases();

    [[nodiscard]] size_t pending_count() const noexcept;
    [[nodiscard]] size_t active_lease_count() const noexcept;
    [[nodiscard]] size_t completed_count() const noexcept;

private:
    [[nodiscard]] static uint64_t current_epoch_sec() noexcept;

    mutable std::mutex mutex_;
    storage::Database& db_;
    SchedulerConfig config_;
    uint64_t next_fence_generation_{1};

    // In-memory queue with SQLite persistence synchronization
    std::vector<ScheduledTask> task_queue_;
    std::unordered_map<std::string, uint64_t> worker_active_leases_;
};

} // namespace dibyt::scheduler
