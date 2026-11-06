#include "dibyt/observability/config.hpp"
#include <cstdlib>

namespace dibyt::observability {

namespace {

std::optional<std::string> get_env_var(const char* name) {
#if defined(_MSC_VER)
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name) == 0 && buf != nullptr) {
        std::string res(buf);
        free(buf);
        return res;
    }
    return std::nullopt;
#else
    const char* val = std::getenv(name);
    if (val != nullptr) {
        return std::string(val);
    }
    return std::nullopt;
#endif
}

} // namespace

core::ErrorCode validate_config(const RuntimeConfig& cfg, std::string& out_error_detail) {
    if (cfg.data_port == 0) {
        out_error_detail = "Data port cannot be 0";
        return core::ErrorCode::kInvalidArgument;
    }
    if (cfg.management_port == 0 || cfg.management_port == cfg.data_port) {
        out_error_detail = "Management port cannot be 0 and cannot equal data port";
        return core::ErrorCode::kInvalidArgument;
    }
    if (cfg.default_workers == 0 || cfg.default_workers > 32) {
        out_error_detail = "Workers must be between 1 and 32";
        return core::ErrorCode::kInvalidArgument;
    }
    if (cfg.chunk_size_bytes < 1024 || cfg.chunk_size_bytes > 16777216) {
        out_error_detail = "Chunk size must be between 1 KiB and 16 MiB";
        return core::ErrorCode::kInvalidArgument;
    }
    if (cfg.checkpoint_interval_bytes < cfg.chunk_size_bytes) {
        out_error_detail = "Checkpoint interval must be greater than or equal to chunk size";
        return core::ErrorCode::kInvalidArgument;
    }
    return core::ErrorCode::kOk;
}

RuntimeConfig load_config_with_env(
    std::optional<std::string_view> data_bind,
    std::optional<uint16_t> data_port,
    std::optional<uint32_t> workers
) {
    RuntimeConfig cfg;

    // 1. Environment variables
    if (auto env_bind = get_env_var("DIBYT_DATA_BIND_ADDRESS")) {
        cfg.data_bind_address = *env_bind;
    }
    if (auto env_port = get_env_var("DIBYT_DATA_PORT")) {
        cfg.data_port = static_cast<uint16_t>(std::stoi(*env_port));
    }
    if (auto env_workers = get_env_var("DIBYT_DEFAULT_WORKERS")) {
        cfg.default_workers = static_cast<uint32_t>(std::stoi(*env_workers));
    }
    if (auto env_chunk = get_env_var("DIBYT_CHUNK_SIZE_BYTES")) {
        cfg.chunk_size_bytes = static_cast<uint32_t>(std::stoul(*env_chunk));
    }
    if (auto env_check = get_env_var("DIBYT_CHECKPOINT_INTERVAL_BYTES")) {
        cfg.checkpoint_interval_bytes = static_cast<uint32_t>(std::stoul(*env_check));
    }

    // 2. Explicit CLI flags override environment variables
    if (data_bind.has_value()) {
        cfg.data_bind_address = std::string(*data_bind);
    }
    if (data_port.has_value()) {
        cfg.data_port = *data_port;
    }
    if (workers.has_value()) {
        cfg.default_workers = *workers;
    }

    return cfg;
}

} // namespace dibyt::observability
