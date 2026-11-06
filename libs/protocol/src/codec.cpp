#include "dibyt/protocol/codec.hpp"
#include <array>

namespace dibyt::protocol {

namespace {

// IEEE 802.3 CRC32 lookup table
constexpr auto make_crc_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            c = (c & 1) ? (0xedb88320L ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
    return table;
}

constexpr auto kCrcTable = make_crc_table();

inline void write_u16_be(uint8_t* dest, uint16_t val) noexcept {
    dest[0] = static_cast<uint8_t>(val >> 8);
    dest[1] = static_cast<uint8_t>(val & 0xFF);
}

inline void write_u32_be(uint8_t* dest, uint32_t val) noexcept {
    dest[0] = static_cast<uint8_t>(val >> 24);
    dest[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    dest[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    dest[3] = static_cast<uint8_t>(val & 0xFF);
}

inline uint16_t read_u16_be(const uint8_t* src) noexcept {
    return static_cast<uint16_t>((static_cast<uint16_t>(src[0]) << 8) | 
                                 static_cast<uint16_t>(src[1]));
}

inline uint32_t read_u32_be(const uint8_t* src) noexcept {
    return (static_cast<uint32_t>(src[0]) << 24) |
           (static_cast<uint32_t>(src[1]) << 16) |
           (static_cast<uint32_t>(src[2]) << 8)  |
           static_cast<uint32_t>(src[3]);
}

} // namespace

uint32_t compute_crc32(std::span<const uint8_t> data) noexcept {
    if (data.empty()) return 0;
    uint32_t c = 0xffffffffL;
    for (uint8_t byte : data) {
        c = kCrcTable[(c ^ byte) & 0xff] ^ (c >> 8);
    }
    return c ^ 0xffffffffL;
}

void encode_header(const FrameHeader& header, uint8_t out_bytes[kHeaderSizeBytes]) noexcept {
    write_u32_be(out_bytes + 0, header.magic);
    write_u16_be(out_bytes + 4, header.version);
    write_u16_be(out_bytes + 6, static_cast<uint16_t>(header.msg_type));
    write_u16_be(out_bytes + 8, header.flags);
    write_u16_be(out_bytes + 10, header.status_code);
    write_u32_be(out_bytes + 12, header.request_id);
    write_u32_be(out_bytes + 16, header.payload_length);
    write_u32_be(out_bytes + 20, header.crc32);
}

core::ErrorCode decode_header(
    std::span<const uint8_t, kHeaderSizeBytes> in_bytes, 
    FrameHeader& out_header
) noexcept {
    uint32_t magic = read_u32_be(in_bytes.data() + 0);
    if (magic != kProtocolMagic) {
        return core::ErrorCode::kProtocolViolation;
    }

    uint16_t version = read_u16_be(in_bytes.data() + 4);
    if (version != kProtocolVersion1) {
        return core::ErrorCode::kProtocolViolation;
    }

    uint16_t raw_type = read_u16_be(in_bytes.data() + 6);
    // Validate known message types
    switch (static_cast<MessageType>(raw_type)) {
        case MessageType::kHandshakeReq:
        case MessageType::kHandshakeResp:
        case MessageType::kManifestReq:
        case MessageType::kManifestResp:
        case MessageType::kLeaseTaskReq:
        case MessageType::kLeaseTaskResp:
        case MessageType::kDataChunk:
        case MessageType::kCheckpointAck:
        case MessageType::kHeartbeat:
        case MessageType::kErrorAbort:
            break;
        default:
            return core::ErrorCode::kProtocolViolation;
    }

    uint32_t payload_len = read_u32_be(in_bytes.data() + 16);
    if (payload_len > kMaxPayloadSizeBytes) {
        // Enforce boundary protection before any buffer allocation
        return core::ErrorCode::kProtocolViolation;
    }

    out_header.magic = magic;
    out_header.version = version;
    out_header.msg_type = static_cast<MessageType>(raw_type);
    out_header.flags = read_u16_be(in_bytes.data() + 8);
    out_header.status_code = read_u16_be(in_bytes.data() + 10);
    out_header.request_id = read_u32_be(in_bytes.data() + 12);
    out_header.payload_length = payload_len;
    out_header.crc32 = read_u32_be(in_bytes.data() + 20);

    return core::ErrorCode::kOk;
}

void encode_frame(const Frame& frame, std::vector<uint8_t>& out_wire_bytes) {
    FrameHeader hdr = frame.header;
    hdr.magic = kProtocolMagic;
    hdr.version = kProtocolVersion1;
    hdr.payload_length = static_cast<uint32_t>(frame.payload.size());
    hdr.crc32 = compute_crc32(frame.payload);

    out_wire_bytes.resize(kHeaderSizeBytes + hdr.payload_length);
    encode_header(hdr, out_wire_bytes.data());

    if (!frame.payload.empty()) {
        std::copy(frame.payload.begin(), frame.payload.end(), out_wire_bytes.begin() + kHeaderSizeBytes);
    }
}

Frame make_error_frame(uint32_t request_id, uint16_t error_code, std::string_view reason) {
    Frame f;
    f.header.msg_type = MessageType::kErrorAbort;
    f.header.request_id = request_id;
    f.header.status_code = error_code;
    f.payload.assign(reason.begin(), reason.end());
    f.header.payload_length = static_cast<uint32_t>(f.payload.size());
    f.header.crc32 = compute_crc32(f.payload);
    return f;
}

} // namespace dibyt::protocol
