# Testing Strategy — Dibyt

**Document Status:** Approved Engineering Strategy (v0.1)

---

## 1. Test Pyramid & Methodology

Dibyt employs a four-tier testing hierarchy to validate functional correctness, fault tolerance, and network robustness:

```text
               / \
              /   \
             / E2E \           Tier 4: Multi-node Network Emulation
            /-------\
           / Fault   \         Tier 3: Chaos & Socket Fault Injection
          / Injection \
         /-------------\
        /  Integration  \      Tier 2: Multi-threaded & Loopback Tests
       /-----------------\
      /     Unit Tests    \    Tier 1: Codecs, Paths, Checksums, Schemas
     /---------------------\
```

---

## 2. Test Tiers Defined

### Tier 1: Unit Tests
- **Framework:** Catch2 or GoogleTest (configured via CMake).
- **Scope:**
  - Protocol frame encoding, decoding, and malformed header rejection.
  - Manifest path normalization and directory traversal rejection.
  - SQLite schema creation, migrations, and query operations.
  - OpenSSL EVP SHA-256 calculation against known vectors.
- **Target Execution:** Sub-second runtimes; executed on every local compilation.

### Tier 2: Integration Tests
- **Scope:**
  - In-process loopback TCP server and client exchange.
  - Handshake negotiation and manifest transmission.
  - Multi-threaded worker pool leasing tasks from an in-memory or SQLite scheduler.
  - Staging file creation and atomic rename verification.

### Tier 3: Fault Injection & Resilience Tests
- **Scope:**
  - Probabilistic socket disconnections during chunk transfer (reproducing legacy `Part2_Case1` & `Part2_Case2` tests).
  - Worker process abrupt SIGKILL / SIGTERM during transfer.
  - Mid-stream byte corruption injection to verify SHA-256 rejection.
  - Sudden server reboot simulation to verify SQLite session persistence.

### Tier 4: End-to-End (E2E) & Benchmark Validation
- **Scope:**
  - Full CLI execution transferring multi-gigabyte collections across realistic network conditions (using `tc` / `netem` for packet loss and latency).
  - Multi-client concurrency tests scaling up to 10–100 simultaneous workers.
  - Resource usage profiling: CPU utilization, memory RSS, file descriptor limits, and I/O wait times.

---

## 3. Truthful Reporting Policy

In accordance with global engineering rules:
1. **Never fabricate test outcomes or benchmark numbers.**
2. A test that was not run must be explicitly labeled `NOT RUN`.
3. Test failures must be investigated and resolved rather than skipped, commented out, or weakened.
4. Benchmarks must document exact hardware specifications, kernel versions, compiler flags, and storage devices.
