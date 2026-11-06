#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <span>
#include "dibyt/core/error.hpp"

namespace dibyt::protocol {

inline constexpr uint32_t kProtocolMagic = 0x44425954; // 'D' 'B' 'Y' 'T'
inline constexpr uint16_t kProtocolVersion1 = 1;
inline constexpr size_t kHeaderSizeBytes = 24;
inline constexpr uint32_t kMaxPayloadSizeBytes = 16777216; // 16 MiB boundary

enum class MessageType : uint16_t {
    kHandshakeReq   = 0x0001,
    kHandshakeResp  = 0x0002,
    kManifestReq    = 0x0010,
    kManifestResp   = 0x0011,
    kLeaseTaskReq   = 0x0020,
    kLeaseTaskResp  = 0x0021,
    kDataChunk      = 0x0030,
    kCheckpointAck  = 0x0031,
    kHeartbeat      = 0x0040,
    kErrorAbort     = 0x00FF
};

struct FrameHeader {
    uint32_t magic{kProtocolMagic};
    uint16_t version{kProtocolVersion1};
    MessageType msg_type{MessageType::kHeartbeat};
    uint16_t flags{0};
    uint16_t status_code{0};
    uint32_t request_id{0};
    uint32_t payload_length{0};
    uint32_t crc32{0};
};

struct Frame {
    FrameHeader header;
    std::vector<uint8_t> payload;
};

// Fast standard CRC32 calculator for payload verification
[[nodiscard]] uint32_t compute_crc32(std::span<const uint8_t> data) noexcept;

// Serialize frame header to 24-byte big-endian buffer
void encode_header(const FrameHeader& header, uint8_t out_bytes[kHeaderSizeBytes]) noexcept;

// Parse 24-byte big-endian buffer into validated frame header
[[nodiscard]] core::ErrorCode decode_header(
    std::span<const uint8_t, kHeaderSizeBytes> in_bytes, 
    FrameHeader& out_header
) noexcept;

// Encode complete frame (header + payload)
void encode_frame(const Frame& frame, std::vector<uint8_t>& out_wire_bytes);

// Helper to construct error abort frame
[[nodiscard]] Frame make_error_frame(uint32_t request_id, uint16_t error_code, std::string_view reason);

} // namespace dibyt::protocol
