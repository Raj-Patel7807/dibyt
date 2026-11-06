# Durable Checkpointing & Reconnect Reconciliation Specification

**Document Status:** Canonical Engineering Specification (Sprint 09)  
**Core Invariant:** The Receiver's Durable Bytes are Authoritative.

---

## 1. Crash Consistency & Failure Boundary Matrix

Every chunk received by a worker passes through 5 discrete execution stages. The table below formalizes system recovery semantics at every possible crash boundary:

| Boundary Stage | Failure Event | Local System State | Recovery / Resumption Behavior |
|---|---|---|---|
| **Boundary 1:** Before staging write | Connection drops or process dies while reading socket. | Nothing written to disk. Receiver SQLite shows previous durable offset $O$. | Safe. Reconnecting worker requests task resumption from offset $O$. Sender re-streams chunk bytes. |
| **Boundary 2:** During staging write | OS halts or SIGKILL mid-`write()`. | Incomplete chunk written to staging file. Checkpoint database not updated. | Safe. On restart, staging file is truncated back to durable offset $O$ stored in SQLite. |
| **Boundary 3:** After write, before `fsync` / Checkpoint | Crash occurs after `write()` returns but before `flush_to_disk()` commits to storage. | Bytes in volatile OS cache are lost on power failure. | Safe. SQLite holds confirmed offset $O$. Reconnection restarts at $O$. |
| **Boundary 4:** After Checkpoint commit, before ACK frame | Crash occurs after SQLite transaction commits $O_{\text{new}}$ but before TCP ACK reaches sender. | Receiver storage and SQLite have committed $O_{\text{new}}$. Sender still believes receiver is at $O$. | Safe. Receiver reconnects and presents authoritative durable offset $O_{\text{new}}$. Sender seeks forward to $O_{\text{new}}$. Zero redundant re-transmission. |
| **Boundary 5:** After ACK, before Sender persistence | Sender crashes before writing ACK to its database. | Receiver is at $O_{\text{new}}$. Sender reloads older state. | Safe. On reconnect, receiver presents $O_{\text{new}}$, which supersedes sender's older state. |

---

## 2. Checkpoint Interval & Durability Policy

1. **Periodic Commit Interval:**
   - Instead of syncing disk after every 64 KiB chunk (which would cause severe disk I/O thrashing), checkpoints commit periodically every **1 MiB** (`1,048,576` bytes) or upon end-of-file.
2. **Durability Order:**
   - `write()` chunk to `.part` file.
   - If interval elapsed: `fsync()` / `FlushFileBuffers()` to physical media.
   - `BEGIN TRANSACTION` -> `UPDATE tasks SET durable_offset = ...` -> `COMMIT`.
   - Transmit `FRAME_CHECKPOINT_ACK` over the data plane TCP connection.
