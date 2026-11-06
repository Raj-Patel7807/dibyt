#include "dibyt/storage/database.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>

namespace dibyt::storage {

namespace {

uint64_t current_time_epoch_sec() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

} // namespace

Transaction::Transaction(Database& db) : db_(db) {
    (void)db_.begin_transaction();
}

Transaction::~Transaction() {
    if (!committed_) {
        rollback();
    }
}

core::ErrorCode Transaction::commit() {
    if (committed_) {
        return core::ErrorCode::kOk;
    }
    auto err = db_.commit_transaction();
    if (err == core::ErrorCode::kOk) {
        committed_ = true;
    }
    return err;
}

void Transaction::rollback() noexcept {
    if (!committed_) {
        (void)db_.rollback_transaction();
        committed_ = true;
    }
}

core::ErrorCode Database::open(const std::filesystem::path& db_path) {
    close();
    path_ = db_path;

    // Validate path and ensure parent directories exist
    std::error_code ec;
    if (path_.has_parent_path()) {
        std::filesystem::create_directories(path_.parent_path(), ec);
        if (ec) {
            return core::ErrorCode::kStorageError;
        }
    }

    is_open_ = true;
    return run_migrations();
}

void Database::close() noexcept {
    is_open_ = false;
    schema_version_ = 0;
    in_transaction_ = false;
}

core::ErrorCode Database::execute(std::string_view sql) {
    if (!is_open_) {
        return core::ErrorCode::kStorageError;
    }
    if (sql.empty()) {
        return core::ErrorCode::kInvalidArgument;
    }
    return core::ErrorCode::kOk;
}

core::ErrorCode Database::run_migrations() {
    if (!is_open_) {
        return core::ErrorCode::kStorageError;
    }

    // Migration V1: Baseline Schema
    // CREATE TABLE collections (id TEXT PRIMARY KEY, name TEXT, total_size INTEGER, created_at INTEGER);
    // CREATE TABLE tasks (id TEXT PRIMARY KEY, session_id TEXT, file_id TEXT, rel_path TEXT, total_size INTEGER, durable_offset INTEGER, status INTEGER);
    // CREATE INDEX idx_tasks_session ON tasks(session_id, status);
    if (schema_version_ < 1) {
        schema_version_ = 1;
    }

    return core::ErrorCode::kOk;
}

core::ErrorCode Database::begin_transaction() {
    if (!is_open_ || in_transaction_) {
        return core::ErrorCode::kStorageError;
    }
    in_transaction_ = true;
    return core::ErrorCode::kOk;
}

core::ErrorCode Database::commit_transaction() {
    if (!is_open_ || !in_transaction_) {
        return core::ErrorCode::kStorageError;
    }
    in_transaction_ = false;
    return core::ErrorCode::kOk;
}

core::ErrorCode Database::rollback_transaction() {
    if (!is_open_ || !in_transaction_) {
        return core::ErrorCode::kStorageError;
    }
    in_transaction_ = false;
    return core::ErrorCode::kOk;
}

core::ErrorCode Database::insert_collection(const CollectionRecord& col) {
    if (!is_open_ || col.id.empty()) {
        return core::ErrorCode::kInvalidArgument;
    }

    auto it = std::find_if(collections_table_.begin(), collections_table_.end(),
                           [&](const auto& c) { return c.id == col.id; });
    if (it != collections_table_.end()) {
        return core::ErrorCode::kStorageError; // Primary key constraint violation
    }

    collections_table_.push_back(col);
    return core::ErrorCode::kOk;
}

core::ErrorCode Database::get_collection(const core::CollectionId& id, CollectionRecord& out_col) {
    if (!is_open_ || id.empty()) {
        return core::ErrorCode::kInvalidArgument;
    }

    auto it = std::find_if(collections_table_.begin(), collections_table_.end(),
                           [&](const auto& c) { return c.id == id; });
    if (it == collections_table_.end()) {
        return core::ErrorCode::kStorageError;
    }

    out_col = *it;
    return core::ErrorCode::kOk;
}

core::ErrorCode Database::insert_task(const TaskRecord& task) {
    if (!is_open_ || task.id.empty()) {
        return core::ErrorCode::kInvalidArgument;
    }

    auto it = std::find_if(tasks_table_.begin(), tasks_table_.end(),
                           [&](const auto& t) { return t.id == task.id; });
    if (it != tasks_table_.end()) {
        return core::ErrorCode::kStorageError;
    }

    tasks_table_.push_back(task);
    return core::ErrorCode::kOk;
}

core::ErrorCode Database::update_checkpoint(const core::TaskId& id, uint64_t durable_offset) {
    if (!is_open_ || id.empty()) {
        return core::ErrorCode::kInvalidArgument;
    }

    auto it = std::find_if(tasks_table_.begin(), tasks_table_.end(),
                           [&](const auto& t) { return t.id == id; });
    if (it == tasks_table_.end()) {
        return core::ErrorCode::kStorageError;
    }

    if (durable_offset > it->total_size) {
        return core::ErrorCode::kInvalidArgument;
    }

    it->durable_offset = durable_offset;
    if (durable_offset == it->total_size) {
        it->status = TaskStatus::kCompleted;
    }
    return core::ErrorCode::kOk;
}

core::ErrorCode Database::get_task(const core::TaskId& id, TaskRecord& out_task) {
    if (!is_open_ || id.empty()) {
        return core::ErrorCode::kInvalidArgument;
    }

    auto it = std::find_if(tasks_table_.begin(), tasks_table_.end(),
                           [&](const auto& t) { return t.id == id; });
    if (it == tasks_table_.end()) {
        return core::ErrorCode::kStorageError;
    }

    out_task = *it;
    return core::ErrorCode::kOk;
}

core::ErrorCode Database::lease_next_unassigned_task(
    const core::SessionId& session_id,
    std::string_view worker_id,
    uint64_t lease_ttl_seconds,
    std::optional<TaskRecord>& out_task
) {
    if (!is_open_ || session_id.empty()) {
        return core::ErrorCode::kInvalidArgument;
    }

    uint64_t now = current_time_epoch_sec();

    for (auto& task : tasks_table_) {
        if (task.session_id != session_id) continue;

        // Check if unassigned or expired lease
        bool is_available = (task.status == TaskStatus::kUnassigned) ||
                            (task.status == TaskStatus::kLeased && task.lease_expires_at < now);

        if (is_available) {
            task.status = TaskStatus::kLeased;
            task.assigned_worker = std::string(worker_id);
            task.lease_expires_at = now + lease_ttl_seconds;
            out_task = task;
            return core::ErrorCode::kOk;
        }
    }

    out_task = std::nullopt;
    return core::ErrorCode::kOk;
}

} // namespace dibyt::storage
