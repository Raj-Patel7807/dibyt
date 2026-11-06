#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <memory>
#include <cstdint>
#include <filesystem>
#include "dibyt/core/error.hpp"
#include "dibyt/core/types.hpp"

namespace dibyt::storage {

enum class TransferStatus : int {
    kCreated = 0,
    kActive = 1,
    kCompleted = 2,
    kFailed = 3,
    kAborted = 4
};

enum class TaskStatus : int {
    kUnassigned = 0,
    kLeased = 1,
    kCompleted = 2,
    kFailed = 3
};

struct CollectionRecord {
    core::CollectionId id;
    std::string name;
    uint64_t total_size_bytes{0};
    uint64_t file_count{0};
    uint64_t created_at{0};
};

struct TaskRecord {
    core::TaskId id;
    core::SessionId session_id;
    core::FileId file_id;
    std::string relative_path;
    uint64_t total_size{0};
    uint64_t durable_offset{0};
    TaskStatus status{TaskStatus::kUnassigned};
    std::string assigned_worker;
    uint64_t lease_expires_at{0};
    uint32_t retry_count{0};
};

// RAII Transaction Manager
class Database;
class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    [[nodiscard]] core::ErrorCode commit();
    void rollback() noexcept;

private:
    Database& db_;
    bool committed_{false};
};

// SQLite-first Persistent Metadata and Checkpoint Storage Engine
class Database {
public:
    Database() = default;
    ~Database() { close(); }

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    [[nodiscard]] core::ErrorCode open(const std::filesystem::path& db_path);
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return is_open_; }
    [[nodiscard]] uint32_t current_schema_version() const noexcept { return schema_version_; }

    // Execute raw SQL statement (DDL / migrations)
    [[nodiscard]] core::ErrorCode execute(std::string_view sql);

    // Schema Migrations Engine
    [[nodiscard]] core::ErrorCode run_migrations();

    // Collection metadata
    [[nodiscard]] core::ErrorCode insert_collection(const CollectionRecord& col);
    [[nodiscard]] core::ErrorCode get_collection(const core::CollectionId& id, CollectionRecord& out_col);

    // Task and Durable Checkpoint records
    [[nodiscard]] core::ErrorCode insert_task(const TaskRecord& task);
    [[nodiscard]] core::ErrorCode update_checkpoint(const core::TaskId& id, uint64_t durable_offset);
    [[nodiscard]] core::ErrorCode get_task(const core::TaskId& id, TaskRecord& out_task);
    [[nodiscard]] core::ErrorCode lease_next_unassigned_task(
        const core::SessionId& session_id,
        std::string_view worker_id,
        uint64_t lease_ttl_seconds,
        std::optional<TaskRecord>& out_task
    );

    // Transaction support
    [[nodiscard]] core::ErrorCode begin_transaction();
    [[nodiscard]] core::ErrorCode commit_transaction();
    [[nodiscard]] core::ErrorCode rollback_transaction();

private:
    std::filesystem::path path_;
    bool is_open_{false};
    uint32_t schema_version_{0};
    bool in_transaction_{false};

    // Internal structured in-memory table mock when native sqlite3 amalgamation is not linked
    std::vector<CollectionRecord> collections_table_;
    std::vector<TaskRecord> tasks_table_;
};

} // namespace dibyt::storage
