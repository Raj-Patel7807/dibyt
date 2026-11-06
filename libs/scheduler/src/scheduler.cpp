#include "dibyt/scheduler/scheduler.hpp"
#include <algorithm>

namespace dibyt::scheduler {

uint64_t TaskScheduler::current_epoch_sec() noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

TaskScheduler::TaskScheduler(storage::Database& db, SchedulerConfig config)
    : db_(db), config_(config) {}

core::ErrorCode TaskScheduler::enqueue_session_tasks(
    const core::SessionId& session_id,
    const std::vector<storage::TaskRecord>& tasks
) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (task_queue_.size() + tasks.size() > config_.max_queue_depth) {
        return core::ErrorCode::kStorageError; // Backpressure limit reached
    }

    for (const auto& tr : tasks) {
        ScheduledTask st;
        st.id = tr.id;
        st.session_id = session_id;
        st.file_id = tr.file_id;
        st.relative_path = tr.relative_path;
        st.total_size_bytes = tr.total_size;
        st.durable_offset = tr.durable_offset;
        st.state = LifecycleState::kPending;
        task_queue_.push_back(std::move(st));
    }

    // Fairness & Makespan Optimization: Sort LPT (Longest Processing Time first)
    std::sort(task_queue_.begin(), task_queue_.end(),
              [](const ScheduledTask& a, const ScheduledTask& b) {
                  return a.total_size_bytes > b.total_size_bytes;
              });

    return core::ErrorCode::kOk;
}

core::ErrorCode TaskScheduler::acquire_lease(
    std::string_view worker_id,
    std::optional<LeaseToken>& out_lease,
    ScheduledTask& out_task
) {
    std::lock_guard<std::mutex> lock(mutex_);
    out_lease = std::nullopt;

    uint64_t now = current_epoch_sec();

    for (auto& task : task_queue_) {
        bool is_claimable = false;

        if (task.state == LifecycleState::kPending || task.state == LifecycleState::kRetryableFailed) {
            is_claimable = true;
        } else if (task.state == LifecycleState::kLeased || task.state == LifecycleState::kReceiving) {
            // Check lease expiration
            if (task.current_lease.expires_epoch_sec < now) {
                is_claimable = true;
            }
        }

        if (is_claimable) {
            LeaseToken token;
            token.task_id = task.id;
            token.worker_id = std::string(worker_id);
            token.lease_epoch_sec = now;
            token.expires_epoch_sec = now + config_.default_lease_ttl_sec;
            token.fence_generation = next_fence_generation_++;

            task.state = LifecycleState::kLeased;
            task.current_lease = token;

            out_lease = token;
            out_task = task;
            return core::ErrorCode::kOk;
        }
    }

    return core::ErrorCode::kOk; // No pending tasks available (idle)
}

core::ErrorCode TaskScheduler::renew_lease(const LeaseToken& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = current_epoch_sec();

    for (auto& task : task_queue_) {
        if (task.id == token.task_id) {
            // Verify fencing generation
            if (task.current_lease.fence_generation != token.fence_generation ||
                task.current_lease.worker_id != token.worker_id) {
                return core::ErrorCode::kTaskLeaseExpired; // Stale worker fenced out
            }

            task.current_lease.expires_epoch_sec = now + config_.default_lease_ttl_sec;
            return core::ErrorCode::kOk;
        }
    }

    return core::ErrorCode::kInvalidArgument;
}

core::ErrorCode TaskScheduler::transition_state(
    const LeaseToken& token, 
    LifecycleState target_state
) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& task : task_queue_) {
        if (task.id == token.task_id) {
            // Validate fencing token
            if (task.current_lease.fence_generation != token.fence_generation ||
                task.current_lease.worker_id != token.worker_id) {
                return core::ErrorCode::kTaskLeaseExpired;
            }

            task.state = target_state;
            return core::ErrorCode::kOk;
        }
    }

    return core::ErrorCode::kInvalidArgument;
}

core::ErrorCode TaskScheduler::complete_task(const LeaseToken& token) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& task : task_queue_) {
        if (task.id == token.task_id) {
            // Strict fencing verification: prevent zombie/stale worker from completing re-assigned task
            if (task.current_lease.fence_generation != token.fence_generation ||
                task.current_lease.worker_id != token.worker_id) {
                return core::ErrorCode::kTaskLeaseExpired;
            }

            task.state = LifecycleState::kComplete;
            return core::ErrorCode::kOk;
        }
    }

    return core::ErrorCode::kInvalidArgument;
}

core::ErrorCode TaskScheduler::report_failure(const LeaseToken& token, bool fatal) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& task : task_queue_) {
        if (task.id == token.task_id) {
            if (task.current_lease.fence_generation != token.fence_generation) {
                return core::ErrorCode::kTaskLeaseExpired;
            }

            task.retry_count++;
            if (fatal || task.retry_count >= task.max_retries) {
                task.state = LifecycleState::kTerminalFailed;
            } else {
                task.state = LifecycleState::kRetryableFailed;
            }
            return core::ErrorCode::kOk;
        }
    }

    return core::ErrorCode::kInvalidArgument;
}

uint32_t TaskScheduler::reclaim_expired_leases() {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = current_epoch_sec();
    uint32_t reclaimed = 0;

    for (auto& task : task_queue_) {
        if ((task.state == LifecycleState::kLeased || task.state == LifecycleState::kReceiving) &&
            task.current_lease.expires_epoch_sec <= now) {
            
            task.retry_count++;
            if (task.retry_count >= task.max_retries) {
                task.state = LifecycleState::kTerminalFailed;
            } else {
                task.state = LifecycleState::kRetryableFailed;
            }
            reclaimed++;
        }
    }

    return reclaimed;
}

size_t TaskScheduler::pending_count() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<size_t>(std::count_if(task_queue_.begin(), task_queue_.end(),
                         [](const ScheduledTask& t) {
                             return t.state == LifecycleState::kPending || 
                                    t.state == LifecycleState::kRetryableFailed;
                         }));
}

size_t TaskScheduler::active_lease_count() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<size_t>(std::count_if(task_queue_.begin(), task_queue_.end(),
                         [](const ScheduledTask& t) {
                             return t.state == LifecycleState::kLeased || 
                                    t.state == LifecycleState::kReceiving ||
                                    t.state == LifecycleState::kVerifying;
                         }));
}

size_t TaskScheduler::completed_count() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<size_t>(std::count_if(task_queue_.begin(), task_queue_.end(),
                         [](const ScheduledTask& t) {
                             return t.state == LifecycleState::kComplete;
                         }));
}

} // namespace dibyt::scheduler
