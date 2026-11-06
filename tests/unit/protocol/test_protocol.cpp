#include <cassert>
#include <iostream>
#include <vector>
#include "dibyt/protocol/codec.hpp"

int main() {
    std::cout << "[TEST] Running Protocol Frame Codec Tests...\n";

    // 1. Valid Frame Round-Trip
    {
        dibyt::protocol::Frame original;
        original.header.msg_type = dibyt::protocol::MessageType::kHandshakeReq;
        original.header.request_id = 42;
        original.header.flags = 0x0001;
        std::string payload_str = "client_hello_v1";
        original.payload.assign(payload_str.begin(), payload_str.end());

        std::vector<uint8_t> wire_bytes;
        dibyt::protocol::encode_frame(original, wire_bytes);

        assert(wire_bytes.size() == dibyt::protocol::kHeaderSizeBytes + payload_str.size());

        // Decode Header
        std::span<const uint8_t, dibyt::protocol::kHeaderSizeBytes> header_span(
            wire_bytes.data(), dibyt::protocol::kHeaderSizeBytes
        );
        dibyt::protocol::FrameHeader decoded_hdr;
        // decode_header returns kOk on valid input; assert the decoded fields
        assert(dibyt::protocol::decode_header(header_span, decoded_hdr) == dibyt::core::ErrorCode::kOk);
        assert(decoded_hdr.magic == dibyt::protocol::kProtocolMagic);
        assert(decoded_hdr.version == dibyt::protocol::kProtocolVersion1);
        assert(decoded_hdr.msg_type == dibyt::protocol::MessageType::kHandshakeReq);
        assert(decoded_hdr.request_id == 42);
        assert(decoded_hdr.payload_length == payload_str.size());

        // Validate CRC32
        std::span<const uint8_t> payload_span(wire_bytes.data() + dibyt::protocol::kHeaderSizeBytes, decoded_hdr.payload_length);
        assert(dibyt::protocol::compute_crc32(payload_span) == decoded_hdr.crc32);
    }

    // 2. Reject Corrupted Magic Bytes
    {
        // Build a header buffer with a deliberately corrupted magic word
        uint8_t bad_magic[dibyt::protocol::kHeaderSizeBytes]{};
        bad_magic[0] = 'X'; // Corrupted magic
        bad_magic[1] = 'X';
        dibyt::protocol::FrameHeader hdr;
        assert(dibyt::protocol::decode_header(bad_magic, hdr) == dibyt::core::ErrorCode::kProtocolViolation);
        (void)bad_magic; // buffer is inspected by decode_header above; suppress unused warning
    }

    // 3. Reject Oversized Payload Length (Pre-allocation Dos Guard)
    {
        dibyt::protocol::FrameHeader fake_hdr;
        fake_hdr.payload_length = 32ULL * 1024ULL * 1024ULL; // 32 MiB (> 16 MiB max)
        uint8_t wire[dibyt::protocol::kHeaderSizeBytes]{};
        dibyt::protocol::encode_header(fake_hdr, wire);

        dibyt::protocol::FrameHeader decoded;
        assert(dibyt::protocol::decode_header(wire, decoded) == dibyt::core::ErrorCode::kProtocolViolation);
    }

    // 4. Reject Unknown Message Type
    {
        dibyt::protocol::FrameHeader fake_hdr;
        fake_hdr.msg_type = static_cast<dibyt::protocol::MessageType>(0x9999);
        uint8_t wire[dibyt::protocol::kHeaderSizeBytes]{};
        dibyt::protocol::encode_header(fake_hdr, wire);

        dibyt::protocol::FrameHeader decoded;
        assert(dibyt::protocol::decode_header(wire, decoded) == dibyt::core::ErrorCode::kProtocolViolation);
    }

    std::cout << "[SMOKE_TEST_PASSED] Protocol codec round-trips and adversarial boundaries verified.\n";
    return 0;
}
