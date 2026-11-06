# Dibyt Wire Protocol Specification (DFP/1)

**Protocol Name:** Dibyt Framing Protocol  
**Current Version:** 1 (`0x0001`)  
**Byte Order:** Big-Endian (Network Byte Order)  
**Status:** Canonical Data-Plane Specification

---

## 1. Frame Envelope Layout

Every frame on the wire begins with an immutable, fixed 24-byte envelope header:

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Magic: 'D' 'B' 'Y' 'T' (0x44 0x42 0x59 0x54)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Version (uint16_t)          |   Message Type (uint16_t)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Flags (uint16_t)            |   Status Code (uint16_t)      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Request ID (uint32_t)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Payload Length (uint32_t)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       CRC32 Checksum (uint32_t)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Payload (0 .. N bytes)                  |
|                                ...                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Exact Envelope Field Widths:
- `magic` (4 bytes): `0x44425954` (`"DBYT"`)
- `version` (2 bytes): `0x0001`
- `msg_type` (2 bytes): Command or response enumeration
- `flags` (2 bytes): Bitmask (`0x0001` = End of Stream, `0x0002` = Compressed)
- `status_code` (2 bytes): `0x0000` = OK / Success; non-zero = error
- `request_id` (4 bytes): Monotonically increasing request correlation ID
- `payload_length` (4 bytes): Payload byte length (bounded: $\le 16\text{ MiB}$)
- `crc32` (4 bytes): CRC32 digest computed over payload bytes (0 for empty payload)

Total Header Size: **24 bytes**.

---

## 2. Message Types Catalogue

| Code (Hex) | Name | Direction | Description |
|---|---|---|---|
| `0x0001` | `HANDSHAKE_REQ` | Client -> Server | Client version, capabilities, and worker authentication token. |
| `0x0002` | `HANDSHAKE_RESP` | Server -> Client | Version acceptance, negotiated parameters, server session ID. |
| `0x0010` | `MANIFEST_REQ` | Client -> Server | Request collection manifest by collection name. |
| `0x0011` | `MANIFEST_RESP` | Server -> Client | Collection catalog, file counts, and file digests. |
| `0x0020` | `LEASE_TASK_REQ` | Client -> Server | Worker requests next transferable task lease. |
| `0x0021` | `LEASE_TASK_RESP`| Server -> Client | Task assignment: File ID, relative path, size, hash, lease TTL. |
| `0x0030` | `DATA_CHUNK` | Server -> Client | Binary chunk containing task ID, byte offset, chunk size, and bytes. |
| `0x0031` | `CHECKPOINT_ACK`| Client -> Server | Receiver confirms durable flush offset on disk. |
| `0x0040` | `HEARTBEAT` | Bi-directional | Keepalive and active lease TTL extension. |
| `0x00FF` | `ERROR_ABORT` | Bi-directional | Fatal protocol error, invalid magic, or session termination. |

---

## 3. Limits & Security Invariants

1. **Pre-Allocation Bounds:** The receiver reads the 24-byte header first. If `payload_length > 16777216` (16 MiB), the socket connection is aborted immediately without allocating a payload buffer.
2. **Endianness Stability:** All multi-byte integers are serialized with explicit bit shifting (`>> 24`, `>> 16`, etc.) ensuring identical wire bytes across x86, ARM, big-endian, and little-endian architectures. Native C++ struct copying (`memcpy`) over sockets is forbidden.
3. **Correlation:** Request IDs allow asynchronous pipelining of multiple task frames over a single TCP stream.
