#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <chrono>
#include <optional>
#include <filesystem>
#include "dibyt/core/error.hpp"

namespace dibyt::observability {

struct RuntimeConfig {
    std::string data_bind_address{"127.0.0.1"};
    uint16_t data_port{9080};
    std::string management_bind_address{"127.0.0.1"};
    uint16_t management_port{9081};

    uint32_t default_workers{4};
    uint32_t max_peer_connections{64};
    uint32_t chunk_size_bytes{65536};            // 64 KiB
    uint32_t checkpoint_interval_bytes{1048576}; // 1 MiB
    uint32_t lease_ttl_seconds{15};
    uint32_t socket_timeout_seconds{30};

    std::filesystem::path db_path{"./dibyt.db"};
    std::filesystem::path staging_dir{"./.dibyt-staging"};

    std::string log_level{"info"};
    std::string log_format{"console"};
    bool tls_enabled{false};
};

// Strict validation of configuration parameters
[[nodiscard]] core::ErrorCode validate_config(
    const RuntimeConfig& cfg, 
    std::string& out_error_detail
);

// Load configuration respecting precedence: CLI > Env > Defaults
[[nodiscard]] RuntimeConfig load_config_with_env(
    std::optional<std::string_view> data_bind = std::nullopt,
    std::optional<uint16_t> data_port = std::nullopt,
    std::optional<uint32_t> workers = std::nullopt
);

} // namespace dibyt::observability
