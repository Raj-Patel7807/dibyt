#pragma once

#include <string_view>
#include <cstdint>

namespace dibyt::core {

// Product identity and version descriptors
inline constexpr std::string_view kProductName = "Dibyt";
inline constexpr std::string_view kProductVersion = "0.1.0-dev";
inline constexpr std::string_view kProductTagline = "Move data. Recover by design.";

// Default operational bounds
inline constexpr uint32_t kDefaultChunkSizeBytes = 65536;        // 64 KiB
inline constexpr uint32_t kDefaultCheckpointIntervalBytes = 1048576; // 1 MiB
inline constexpr uint32_t kDefaultLeaseTtlSeconds = 15;
inline constexpr uint32_t kMaxFramePayloadBytes = 16777216;      // 16 MiB

} // namespace dibyt::core
