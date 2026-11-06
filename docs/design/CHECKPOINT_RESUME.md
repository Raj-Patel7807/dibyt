# Checkpoint & Resume Design — Dibyt

**Document Status:** Approved Design Specification (v0.1)

---

## 1. The Core Invariant: Receiver Authority

In Dibyt, **the receiver's durable bytes are the sole source of truth**.

A sender process must never assume that sending bytes across a TCP socket implies successful receipt or durability on the receiving host. Data in transit may be queued in OS socket send buffers, delayed in network switches, or dropped if the receiver crashes before writing to disk.

### The Invariant
> **No progress is acknowledged to the sender, and no checkpoint is recorded, until the receiver has written the data to local storage and successfully executed a durable synchronization step.**

---

## 2. Checkpoint Workflow & Invariant Sequence

Every worker transfer stream follows this strict transactional sequence:

```text
Sender                                                      Receiver
  |                                                            |
  | -------- FRAME_DATA_CHUNK [Offset O, Len L, Payload] ----> |
  |                                                            | 1. Write chunk to staging file
  |                                                            | 2. Accumulate received bytes
  |                                                            | 3. Has checkpoint interval (1MB) elapsed?
  |                                                            |    YES ->
  |                                                            | 4. Flush OS buffers (fsync / fdatasync)
  |                                                            | 5. BEGIN SQLite Transaction
  |                                                            |    UPDATE tasks SET durable_offset = O + L
  |                                                            |    COMMIT
  | <------- FRAME_CHECKPOINT_ACK [TaskID, DurableOffset] ---- | 6. Send durable ACK frame
  |                                                            |
  | 7. Record durable ACK in sender DB                        |
```

### Critical Rules
1. **Flush Before ACK:** If the receiver crashes between step 1 and step 4, the un-flushed bytes are discarded on restart. The database will reflect the last durable offset, and resumption will restart cleanly from that offset.
2. **Atomic SQLite State:** Checkpoint offsets are recorded in the receiver's SQLite metadata store using single-statement or explicit atomic transactions.
3. **Sender Persistence:** When the sender receives `FRAME_CHECKPOINT_ACK`, it updates its lease record to note confirmed progress. If the sender process restarts, it will require incoming reconnecting workers to present their durable offset.

---

## 3. Resume & Reconnection Protocol

When a network drop, timeout, or process restart occurs, the resumption sequence is deterministic:

1. **Worker Reconnects:** The receiver initiates a new TCP connection to the sender.
2. **Session Handshake:**
   - Worker sends `FRAME_SESSION_RESUME` with `SessionUUID`, `WorkerID`, and authentication token.
   - Sender validates that the session is active and not aborted.
3. **Task State Query:**
   - Receiver inspects local SQLite database for the highest confirmed `durable_offset` for the leased file task.
   - Receiver sends `FRAME_TASK_RESUME` specifying `TaskID` and `ConfirmedOffset`.
4. **Sender Offset Validation:**
   - Sender validates that `ConfirmedOffset <= FileSize` and that `ConfirmedOffset` matches known progression bounds.
   - Sender uses file seeking (`lseek()` / `pread()`) to jump directly to `ConfirmedOffset`.
5. **Stream Resumption:**
   - Sender resumes streaming chunks starting at `ConfirmedOffset`.
   - Receiver opens the local staging file in append/update mode (`O_WRONLY | O_CREAT`), seeks to `ConfirmedOffset`, and resumes writing.

---

## 4. File Quarantine & Safe Finalization

Under no circumstances is a partially downloaded file visible at its destination path.

### Quarantine Lifecycle
1. **Creation:** When a file task begins, the receiver creates a staging file in a hidden directory:
   ```text
   <DestinationRoot>/.dibyt-staging/<SessionUUID>/<FileUUID>.part
   ```
2. **Transfer:** All incoming byte chunks and checkpoint flushes occur exclusively on this `.part` staging file.
3. **Verification:** When `durable_offset == total_size`, the receiver computes the full SHA-256 digest over the `.part` file using OpenSSL EVP.
4. **Comparison:** The computed hash is compared with the authoritative SHA-256 in the manifest.
5. **Atomic Rename:**
   - If hashes match: the staging file is moved to `<DestinationRoot>/<RelativePath>` via an atomic filesystem operation (`std::filesystem::rename` / `renameat`).
   - If directory parents do not exist, they are created with restricted permissions before the rename.
6. **Rejection:**
   - If the hash mismatches: the `.part` file is quarantined or unlinked, the SQLite task is reset to `PENDING` with an incremented failure count, and the error is logged.
