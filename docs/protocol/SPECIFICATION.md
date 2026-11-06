# Dibyt Binary Wire Protocol Specification (Draft)

**Protocol Name:** Dibyt Framing Protocol (DFP)  
**Version:** 1 (Draft / In-Design)  
**Target Milestone:** Sprint 05  
**Document Status:** Working Specification Draft (Not Yet Implemented in Code)

---

## 1. Design Goals

1. **Explicit Length Framing:** Sockets are stream-oriented; every frame has an explicit length header. No reliance on ASCII delimiters (such as newlines) for binary streams.
2. **Fixed Header Envelope:** Every frame begins with a fixed 16-byte binary header. No variable-width encoding for envelope metadata.
3. **Network Byte Order:** All integer fields in headers are encoded in big-endian (network byte order).
4. **Strict Frame Limits:** To prevent remote denial-of-service or memory-exhaustion exploits, payload sizes are capped at 16 MiB (`16,777,216` bytes). Frames exceeding this boundary are rejected, and the socket is immediately terminated.
5. **Version Negotiation:** Senders and receivers negotiate the highest mutually supported protocol version during the initial handshake frame.

---

## 2. Frame Structure

Every frame transmitted over a Dibyt TCP data connection conforms to the following fixed layout:

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Magic: 'D' 'B' 'Y' 'T' (0x44 0x42 0x59 0x54)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Version (16 bits)           |   Message Type (16 bits)      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Flags (16 bits)             |   Reserved / Status (16 bits) |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Payload Length (32 bits)                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Payload (0 to N bytes)                  |
|                               ...                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Header Field Definitions:
- **Magic Bytes (32 bits):** `0x44425954` (`"DBYT"` in ASCII). Sockets receiving anything else immediately abort the connection.
- **Protocol Version (16 bits):** Current version `0x0001`.
- **Message Type (16 bits):** Enumerated command or response type.
- **Flags (16 bits):** Bitmask for frame attributes (e.g., `0x0001` = End of Stream, `0x0002` = Compressed).
- **Reserved / Status Code (16 bits):** Used in response frames to signal error codes (`0x0000` = SUCCESS).
- **Payload Length (32 bits):** Size in bytes of the following payload. Must be $\le 16\,777\,216$.
- **Payload ($N$ bytes):** Binary body formatted according to `Message Type`.

---

## 3. Message Type Enumeration

| Code (Hex) | Message Name | Direction | Description |
|---|---|---|---|
| `0x0001` | `HANDSHAKE_REQ` | Client -> Server | Initial connection, protocol version, client ID. |
| `0x0002` | `HANDSHAKE_RESP` | Server -> Client | Handshake acceptance, selected version, server capabilities. |
| `0x0010` | `MANIFEST_REQ` | Client -> Server | Request collection manifest. |
| `0x0011` | `MANIFEST_RESP` | Server -> Client | Collection metadata and file list. |
| `0x0020` | `LEASE_TASK_REQ` | Client -> Server | Worker requests next available file task. |
| `0x0021` | `LEASE_TASK_RESP`| Server -> Client | Task assignment (File ID, path, size, hash, lease TTL). |
| `0x0030` | `DATA_CHUNK` | Server -> Client | Stream of payload bytes with offset and chunk length. |
| `0x0031` | `CHECKPOINT_ACK`| Client -> Server | Durable byte offset confirmation from receiver. |
| `0x0040` | `HEARTBEAT` | Both | Keepalive and lease extension frame. |
| `0x00FF` | `ERROR_ABORT` | Both | Fatal protocol or application error frame. |

---

## 4. Error Handling & Connection Lifecycle

1. **Header Decode Validation:**
   - If magic bytes do not equal `0x44425954`, close connection immediately (possible port scanner or corrupted stream).
   - If version is unsupported, reply with `ERROR_ABORT (ERR_UNSUPPORTED_VERSION)` and close connection.
   - If payload length exceeds maximum allowed frame size (16 MiB), abort connection immediately.
2. **Read Loops:**
   - Always invoke `recv_exact()` until the complete 16-byte header is read.
   - Allocate payload buffers only after verifying payload length against maximum limits.
   - Read exact payload size; any EOF before full payload arrival is treated as an abrupt connection termination.
