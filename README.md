# Dibyt — Move data. Recover by design.

[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![Status](https://img.shields.io/badge/status-foundation--documentation-orange.svg)](docs/planning/ROADMAP.md)

Dibyt is an open, self-hosted file distribution system built in modern C++ for resilient, parallel, and resumable data transfers over raw TCP. It enables senders to publish arbitrary file collections and receivers to ingest them concurrently across multiple bounded network streams with durable progress tracking, automatic fault recovery, and end-to-end cryptographic integrity verification.

Dibyt is **not** a cloud storage platform, an object storage emulator, or a P2P overlay. It operates directly between self-managed servers, edge nodes, and developer workstations.

> **Status Notice:** Dibyt is currently in its repository foundation and documentation phase (Sprint 01). No product application code has been released yet. Academic predecessor prototypes are cataloged under historical documentation and will be safely preserved in subsequent sprints. See the [Roadmap](docs/planning/ROADMAP.md) for progress across the 20 engineering sprints.

---

## The Problem

Transferring large collections of files over networks frequently fails due to Wi-Fi drops, transient network partitions, worker process terminations, or host reboots. Conventional transfer approaches often:
1. **Restart from byte zero:** Discarding gigabytes of already transferred bytes upon connection interruption.
2. **Lack concurrency or overload endpoints:** Unbounded thread-per-connection patterns risk resource exhaustion, while single-stream transfers fail to saturate high-latency links.
3. **Compromise integrity:** Partial or unverified bytes end up exposed directly in final destination folders before SHA-256 verification completes.
4. **Rely on volatile state:** Progress stored exclusively in process memory is obliterated on unexpected daemon restarts.

Dibyt solves these problems by design:
- **Parallel TCP Transfers:** Dynamic worker pools lease transfer tasks and pull file payloads across multiple TCP streams.
- **Durable Checkpoints:** Receivers maintain transactional SQLite-backed state and flush confirmed byte offsets before acknowledging progress.
- **Fail-Safe Integrity Finalization:** Transferred files remain in quarantine as temporary files until their full SHA-256 hash matches the manifest, followed by an atomic rename into destination roots.
- **Independent Worker Recovery:** If a single worker connection drops, surviving workers continue transferring unaffected tasks while the failed task is reassigned.

---

## Architectural Overview

Dibyt is architected as a **modular monolith** with clean separation between the control plane and data plane:

```text
+-----------------------------------------------------------------+
|                         Control Plane                           |
|   CLI (CLI11) / HTTP Management API / Status / Config / Auth    |
+-----------------------------------------------------------------+
                                |
                                v
+-----------------------------------------------------------------+
|                          Data Plane                             |
|       Custom TCP Protocol / Length-Framed Messages / TLS        |
|  [Worker 1]        [Worker 2]        [Worker 3]     ... [Pool]  |
+-----------------------------------------------------------------+
        |                    |                     |
        v                    v                     v
+----------------+   +-----------------+   +---------------------+
| Dynamic Lease  |   | SQLite Storage  |   | Temporary Directory |
| Task Scheduler |   | Durable State   |   | -> Atomic SHA-256   |
+----------------+   +-----------------+   +---------------------+
```

- **Control Plane:** Handles transfer creation, configuration, status reporting, and authentication.
- **Data Plane:** Handles high-throughput TCP streaming, binary frame serialization, and checksum calculation. Bulk payloads never transit HTTP management endpoints.
- **Bounded Concurrency:** System limits strictly bound parallel connections, queue sizes, memory buffers, and retry limits.

Read more in the [Architecture Overview](docs/architecture/ARCHITECTURE.md).

---

## Implementation & Feature Status

| Capability / Module | Target Milestone | Status | Description |
|---|---|---|---|
| **Repository & Documentation Foundation** | Sprint 01 | **Implemented** | Complete specifications, ADRs, threat models, and legal documentation. |
| **Legacy Coursework Archival** | Sprint 02 | Planned | Safe relocation of academic prototype files into `legacy/academic/cn-project/`. |
| **CMake & Dependency Build System** | Sprint 03 | Planned | Modern C++20 CMake build configuration, presets, and toolchain policies. |
| **Manifest & Collection Model** | Sprint 04 | Planned | Canonical relative path sanitization, traversal defense, and SHA-256 calculation. |
| **Binary Protocol & Frame Codec** | Sprint 05 | Planned | Fixed header framing, magic bytes, length validation, and error frames. |
| **TCP Transport & Connection Lifecycle** | Sprint 06 | Planned | RAII socket management, bounded I/O loops, and timeout enforcement. |
| **Integrity & Safe Finalization** | Sprint 07 | Planned | Temporary staging files, OpenSSL EVP hashing, and atomic rename. |
| **SQLite Persistence & Migrations** | Sprint 08 | Planned | Transactional state database, schema migration engine, and crash recovery. |
| **Checkpoint & Resume Engine** | Sprint 09 | Planned | Acknowledged durable byte progression and offset-based streaming. |
| **Dynamic Work Scheduler** | Sprint 10 | Planned | Leased task queue replacing naive modulo scheduling. |
| **End-to-End Parallel TCP Transfer** | Sprint 11 | Planned | Multi-connection transfers with bounded concurrency (10–100 peer target). |
| **Fault Injection & Recovery Engine** | Sprint 12 | Planned | Resilience testing against drops, process restarts, and network partitions. |
| **Production CLI** | Sprint 13 | Planned | User-friendly `dibyt send`, `dibyt receive`, and `dibyt status` CLI commands. |
| **Observability & Metrics** | Sprint 14 | Planned | Structured logging, Prometheus metrics, and health diagnostics. |
| **Security & TLS Encryption** | Sprint 15 | Planned | Mutual TLS, filesystem root sandboxing, and token authentication. |
| **Management REST API** | Sprint 16 | Planned | Lightweight C++ REST API for external orchestrators and dashboards. |
| **Web Dashboard** | Sprint 17 | Planned | React + TypeScript web UI for real-time monitoring and management. |
| **Benchmarking & Validation** | Sprint 18 | Planned | Rigorous controlled benchmarks with reproducible test matrices. |
| **Packaging & Containerization** | Sprint 19 | Planned | Multi-stage Docker containers, Compose setups, and CI automation. |
| **Final Hardening & Audit** | Sprint 20 | Planned | Security audit, static analysis, and v0.1 release packaging. |

---

## Historical Academic Prototype

Dibyt originates from an academic Computer Networks socket programming project (`IT305`, Groups 056 & 069), which explored four staged socket transfer implementations:
1. `Part1`: Sequential vs. multi-threaded baseline transfer of topic-mapped directory files.
2. `Part2_Case1`: Probabilistic socket disconnection with restart-from-zero behavior.
3. `Part2_Case2`: Checkpoint-based byte-offset resume using text-based session state.
4. `Part2_Case2_Enhanced`: Multi-connection parallel transfers with deterministic modulo file partitioning.

**Observed Limitations of the Academic Prototype:**
- Session state was maintained strictly in-memory on the server; server restarts caused permanent session loss.
- Partitioning files via `index % num_workers` caused severe transfer time imbalances on uneven file sizes.
- No end-to-end cryptographic hash (SHA-256) verification contract existed.
- Path sanitization relied on a simple `safe_name()` string check rather than strict canonical directory boundary containment.
- Sockets and concurrency were POSIX/Linux-specific without formal build systems or automated tests.

These historical artifacts are preserved as academic history in `legacy/academic/cn-project/` (scheduled for Sprint 02). See [Historical Prototype Analysis](docs/design/LEGACY_ANALYSIS.md).

---

## Platform Support

- **Target OS:** Linux (x86_64, aarch64) as primary development and deployment target.
- **Experimental / Future:** macOS and Windows (via POSIX portability abstraction layers in later sprints).
- **Toolchain:** C++20 compliant compiler (`gcc` 11+, `clang` 14+), CMake 3.22+, Ninja / Make.

---

## Build & Usage (Pending)

> **Notice:** The product build system and CLI are scheduled for implementation in Sprint 03 through Sprint 13. Build commands will be documented here once CMake and binary targets are established.

---

## Documentation Index

Comprehensive engineering and product documentation is available in [`docs/`](docs/README.md):
- [Product Requirements & Vision](docs/product/PRD.md)
- [System Architecture](docs/architecture/ARCHITECTURE.md)
- [Protocol Specification Draft](docs/protocol/SPECIFICATION.md)
- [Checkpoint & Resume Design](docs/design/CHECKPOINT_RESUME.md)
- [Threat Model & Security Gates](docs/security/THREAT_MODEL.md)
- [Development Standards](docs/development/STANDARDS.md)
- [Sprint Roadmap (01–20)](docs/planning/ROADMAP.md)
- [Architecture Decision Records (ADRs)](docs/decisions/README.md)

---

## Contributing

We welcome community feedback and contributions. Please read [CONTRIBUTING.md](CONTRIBUTING.md), [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md), and [SECURITY.md](SECURITY.md) before submitting pull requests.

---

## License & Legal Notices

Dibyt core source code and documentation created for the Dibyt product are licensed under the **Apache License, Version 2.0**. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

Academic predecessor coursework materials preserved in the repository carry specific academic attribution and rights boundaries. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [LEGAL_AND_RIGHTS.md](docs/LEGAL_AND_RIGHTS.md) for full details.
