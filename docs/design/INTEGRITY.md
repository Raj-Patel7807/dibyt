# Integrity & Finalization Design — Dibyt

**Document Status:** Approved Design Specification (v0.1)

---

## 1. Cryptographic Hash Pipeline

In Dibyt, every file published in a collection manifest is fingerprinted with an authoritative cryptographic hash: **SHA-256** computed via OpenSSL EVP (`EVP_DigestInit_ex`, `EVP_DigestUpdate`, `EVP_DigestFinal_ex`).

### Verification Rules
1. **Incremental Streaming Hash:** As bytes are read by the sender, or as chunks are written by the receiver, the SHA-256 context can be updated incrementally without loading entire multi-gigabyte files into system RAM.
2. **Post-Transfer Verification:** Once all bytes of a file have been written and flushed to the `.part` file, the receiver computes a final, fresh SHA-256 pass over the complete file on disk to guarantee zero memory-corruption artifacts before finalization.
3. **Hexadecimal String Canonicalization:** SHA-256 hashes are serialized across protocols and manifests as 64-character lowercase hexadecimal strings.

---

## 2. Safe Staging & Atomic Directory Finalization

```text
Staging Directory (Quarantine)                Destination Root
==============================                ================
.dibyt-staging/<session_id>/
  ├── file_001.part  --[Hash Verified OK]-->  ├── /models/weights.bin (Atomic rename)
  └── file_002.part  --[Hash Mismatch!] --->  [DELETED / QUARANTINED] (Task Re-queued)
```

### Safety Invariants:
1. **Never Expose Incomplete Files:** Downstream consumers (users, scripts, CI systems) monitoring the destination directory will never observe a partially downloaded file under its final name.
2. **Atomic Move:** Renaming a file on the same filesystem is an atomic POSIX operation (`rename(2)` / `std::filesystem::rename`). The staging directory must be provisioned on the **same physical filesystem mount** as the target destination to guarantee atomicity and prevent expensive cross-device copies.
3. **Pre-Creation of Parent Directories:** Before executing the atomic move, the receiver creates all required parent directories (e.g., `mkdir -p`) with restrictive permissions (`0750`).
4. **Collision Handling:** If a file already exists at the destination path:
   - Default behavior: Abort or fail the task unless an explicit `--overwrite` policy flag was supplied during session initialization.
   - If overwriting is permitted, the existing file is replaced atomically by the verified staging file.
