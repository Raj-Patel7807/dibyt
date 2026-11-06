#include "dibyt/transfer/engine.hpp"
#include <iostream>
#include <fstream>

namespace dibyt::transfer {

TransferEngine::TransferEngine(storage::Database& db, EngineConfig config)
    : db_(db), config_(config), scheduler_(db) {}

core::ErrorCode TransferEngine::execute_local_parallel_transfer(
    const std::filesystem::path& source_root,
    const std::filesystem::path& destination_root,
    const manifest::Manifest& manifest
) {
    if (manifest.files.empty()) {
        return core::ErrorCode::kOk;
    }

    core::SessionId session_id("sess_" + manifest.collection_name);

    // 1. Convert manifest entries to storage tasks
    std::vector<storage::TaskRecord> tasks;
    tasks.reserve(manifest.files.size());
    for (size_t i = 0; i < manifest.files.size(); ++i) {
        const auto& f = manifest.files[i];
        storage::TaskRecord tr;
        tr.id = core::TaskId("t_" + std::to_string(i) + "_" + f.id.value());
        tr.session_id = session_id;
        tr.file_id = f.id;
        tr.relative_path = f.relative_path;
        tr.total_size = f.size_bytes;
        tr.durable_offset = 0;
        tr.status = storage::TaskStatus::kUnassigned;
        tasks.push_back(std::move(tr));
    }

    // 2. Enqueue tasks in dynamic scheduler (applies LPT ordering)
    auto err = scheduler_.enqueue_session_tasks(session_id, tasks);
    if (err != core::ErrorCode::kOk) {
        return err;
    }

    std::atomic<bool> abort_signal{false};
    std::atomic<uint32_t> files_completed{0};

    // 3. Spawn bounded worker thread pool
    uint32_t worker_count = std::clamp(config_.num_workers, 1u, 32u);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    for (uint32_t i = 0; i < worker_count; ++i) {
        workers.emplace_back(&TransferEngine::worker_transfer_loop, this, i,
                             std::cref(source_root), std::cref(destination_root),
                             std::ref(abort_signal), std::ref(files_completed));
    }

    // 4. Await completion of all worker threads
    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    if (abort_signal.load()) {
        return core::ErrorCode::kStorageError;
    }

    return core::ErrorCode::kOk;
}

void TransferEngine::worker_transfer_loop(
    uint32_t worker_idx,
    const std::filesystem::path& source_root,
    const std::filesystem::path& destination_root,
    std::atomic<bool>& abort_signal,
    std::atomic<uint32_t>& files_completed
) {
    std::string worker_id = "worker_" + std::to_string(worker_idx);

    while (!abort_signal.load()) {
        std::optional<scheduler::LeaseToken> lease;
        scheduler::ScheduledTask task;

        auto err = scheduler_.acquire_lease(worker_id, lease, task);
        if (err != core::ErrorCode::kOk || !lease.has_value()) {
            // No more work in queue
            break;
        }

        // Open source file
        std::filesystem::path src_file = source_root / std::filesystem::path(task.relative_path);
        std::ifstream src(src_file, std::ios::binary);
        if (!src.is_open()) {
            (void)scheduler_.report_failure(*lease, true);
            abort_signal.store(true);
            break;
        }

        // Compute expected SHA-256 for the source file
        std::string expected_sha256;
        err = manifest::compute_file_sha256(src_file, expected_sha256);
        if (err != core::ErrorCode::kOk) {
            (void)scheduler_.report_failure(*lease, true);
            abort_signal.store(true);
            break;
        }

        // Open destination staging writer
        StagedFileWriter writer;
        err = writer.open(
            destination_root, 
            task.relative_path, 
            expected_sha256, 
            task.total_size_bytes, 
            config_.overwrite_policy
        );
        if (err != core::ErrorCode::kOk) {
            (void)scheduler_.report_failure(*lease, true);
            abort_signal.store(true);
            break;
        }

        // Stream chunks
        std::vector<uint8_t> buffer(config_.chunk_size_bytes);
        bool transfer_ok = true;

        while (src.good() && !abort_signal.load()) {
            src.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
            std::streamsize bytes_read = src.gcount();
            if (bytes_read > 0) {
                std::span<const uint8_t> chunk(buffer.data(), static_cast<size_t>(bytes_read));
                if (writer.append_chunk(chunk) != core::ErrorCode::kOk) {
                    transfer_ok = false;
                    break;
                }
            }
        }

        if (!transfer_ok || abort_signal.load()) {
            writer.abort();
            (void)scheduler_.report_failure(*lease, false);
            break;
        }

        // Flush and finalize (cryptographic SHA-256 validation + atomic rename)
        err = writer.finalize();
        if (err != core::ErrorCode::kOk) {
            writer.abort();
            (void)scheduler_.report_failure(*lease, true);
            abort_signal.store(true);
            break;
        }

        // Mark task complete with matching fence token
        err = scheduler_.complete_task(*lease);
        if (err != core::ErrorCode::kOk) {
            abort_signal.store(true);
            break;
        }

        files_completed.fetch_add(1);
    }
}

} // namespace dibyt::transfer
