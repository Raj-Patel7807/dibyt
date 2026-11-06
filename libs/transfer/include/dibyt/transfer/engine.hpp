#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <filesystem>
#include "dibyt/core/error.hpp"
#include "dibyt/manifest/manifest.hpp"
#include "dibyt/networking/tcp.hpp"
#include "dibyt/storage/database.hpp"
#include "dibyt/transfer/staging.hpp"
#include "dibyt/scheduler/scheduler.hpp"

namespace dibyt::transfer {

struct EngineConfig {
    uint32_t num_workers{4};
    uint32_t chunk_size_bytes{65536}; // 64 KiB
    uint64_t checkpoint_interval_bytes{1048576}; // 1 MiB
    OverwritePolicy overwrite_policy{OverwritePolicy::kDisallow};
};

// End-to-end parallel transfer orchestrator
class TransferEngine {
public:
    TransferEngine(storage::Database& db, EngineConfig config = {});
    ~TransferEngine() = default;

    TransferEngine(const TransferEngine&) = delete;
    TransferEngine& operator=(const TransferEngine&) = delete;

    // Executes parallel fetch transfer of an entire collection from a source directory to a target directory
    [[nodiscard]] core::ErrorCode execute_local_parallel_transfer(
        const std::filesystem::path& source_root,
        const std::filesystem::path& destination_root,
        const manifest::Manifest& manifest
    );

private:
    void worker_transfer_loop(
        uint32_t worker_idx,
        const std::filesystem::path& source_root,
        const std::filesystem::path& destination_root,
        std::atomic<bool>& abort_signal,
        std::atomic<uint32_t>& files_completed
    );

    storage::Database& db_;
    EngineConfig config_;
    scheduler::TaskScheduler scheduler_;
};

} // namespace dibyt::transfer
