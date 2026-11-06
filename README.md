# Dibyt — Move data. Recover by design.

[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](.github/workflows/ci.yml)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)
[![Status](https://img.shields.io/badge/status-developer--preview-green.svg)](docs/operations/PRODUCTION-READINESS-CHECKLIST.md)

Dibyt is an open, self-hosted file distribution system built in modern C++20 for resilient, parallel, and resumable data transfers over raw TCP. It enables senders to publish arbitrary file collections and receivers to ingest them concurrently across multiple bounded network streams with durable progress tracking, automatic fault recovery, and end-to-end cryptographic integrity verification.

Dibyt is **not** a cloud storage platform, an object storage emulator, or a P2P overlay. It operates directly between self-managed servers, edge nodes, and developer workstations.

> **Status:** All 20 engineering sprints are implemented and verified. The codebase is currently in **Developer Preview** (`v0.1.0-dev`). See the [Roadmap](docs/planning/ROADMAP.md) and [Production Readiness Checklist](docs/operations/PRODUCTION-READINESS-CHECKLIST.md).

---

## Key Capabilities & Invariants

- **Multi-Stream Parallel TCP Transfers:** Dynamic worker pools lease transfer tasks and pull file payloads concurrently across bounded TCP connections without thread exhaustion.
- **Durable Checkpoints (Receiver Authority):** Transfer progress is authoritative only when durably synced (`flush`) and committed to SQLite by the receiver before acknowledging progress.
- **Fail-Safe Integrity Quarantine:** Files stream into quarantine staging directories (`.staging/`) with temporary names. Final atomic promotion (`rename`) occurs strictly after full SHA-256 verification against the manifest.
- **Dynamic Task Scheduler with Lease Fencing:** Dynamic LPT (Longest Processing Time first) scheduler assigns chunk tasks with monotonic `fence_generation` lease tokens, strictly preventing stale or zombie workers from corrupting state upon reconnect.
- **Strict Plane Separation:** High-speed bulk binary payloads transfer over raw TCP (port 9080) with 32-byte framing; administrative control, Prometheus metrics, and status endpoints operate over HTTP/JSON (port 8080).
- **Embedded Zero-Dependency Storage:** Full ACID transactional durability powered by embedded SQLite in WAL mode with automated migrations.
- **Defense-in-Depth Security:** Constant-time token authentication (`constant_time_equals`), default-deny authorization, and canonical path containment (`FilesystemJail`) blocking directory traversal attacks (`../`, symlinks).
- **Operational Ergonomics:** Single-binary CLI (`dibyt publish`, `fetch`, `status`), unprivileged multi-stage Docker container (`UID 10001`), Docker Compose, systemd unit, and React 19 web console dashboard.

---

## Architectural Overview

Dibyt is architected as a **modular monolith** with clean separation between the control plane and data plane:

```text
+-------------------------------------------------------------------------+
|                              Control Plane                              |
|   CLI (apps/dibyt-cli) / HTTP REST API (apps/dibyt-api) / Web Dashboard |
|         - Port 8080: /health, /metrics, /api/v1/collections             |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
|                               Data Plane                                |
|           Binary Framing Codec (32-byte header, CRC32, SHA-256)         |
|  [Worker 1 (TCP)]    [Worker 2 (TCP)]    [Worker 3 (TCP)]   (Port 9080) |
+-------------------------------------------------------------------------+
         |                    |                     |
         v                    v                     v
+----------------+   +-----------------+   +---------------------+
| Dynamic Lease  |   | SQLite Storage  |   | Staging Quarantine  |
| Task Scheduler |   | Durable State   |   | -> Atomic SHA-256   |
+----------------+   +-----------------+   +---------------------+
```

Read more in the [Architecture Overview](docs/architecture/ARCHITECTURE.md) and [Architecture Decision Records](docs/decisions/01-ARCHITECTURE-DECISIONS.md).

---

## Implementation & Sprint Roadmap Status

All 20 roadmap sprints have been fully implemented, tested, and audited on `main`:

| Sprint | Subsystem / Milestone | Deliverable | Status |
|---|---|---|:---:|
| **01** | Documentation Foundation | PRD, Architecture, Threat Model, Standards | **Done** |
| **02** | Legacy Coursework Archival | Relocated prototype into `legacy/academic/cn-project/` | **Done** |
| **03** | CMake Build Foundation | ISO C++20 toolchain, presets, and strict warnings | **Done** |
| **04** | Core Domain & Manifest | Path sanitization, collection scanner, SHA-256 | **Done** |
| **05** | Protocol Wire Codec | Binary framing (32-byte header), CRC32 checksums | **Done** |
| **06** | Networking & TCP Transport | RAII socket management, bounded I/O polling | **Done** |
| **07** | Integrity & Safe Finalization | Staging quarantine, SHA-256 verification, atomic rename | **Done** |
| **08** | SQLite Persistence | Transactional state database, automated migrations | **Done** |
| **09** | Durable Checkpoints | Flush-before-ACK invariant, stream recovery | **Done** |
| **10** | Dynamic Work Scheduler | LPT task scheduling, monotonic lease fencing | **Done** |
| **11** | End-to-End Parallel Transfer | Multi-worker parallel transfer engine | **Done** |
| **12** | Fault Recovery & Retries | Error taxonomy, jittered exponential backoff | **Done** |
| **13** | Production CLI Experience | Subcommands (`publish`, `fetch`, `status`), JSON output | **Done** |
| **14** | Observability & Metrics | Structured JSON logging, secret redaction, metrics | **Done** |
| **15** | Security Foundations | Constant-time authentication, filesystem jail | **Done** |
| **16** | Management REST API | `/api/v1` router, OpenAPI 3.0 specification | **Done** |
| **17** | Web Management Dashboard | React 19 + TypeScript real-time console | **Done** |
| **18** | Testing, Chaos & Benchmarks | Fault injection test suite, reproducible benchmark runner | **Done** |
| **19** | Docker & Self-Hosted Deployment | Multi-stage Dockerfiles, Docker Compose, systemd unit | **Done** |
| **20** | Final Security Audit & Release Gate | Final audit report, readiness checklist, SBOM | **Done** |

See [ROADMAP.md](docs/planning/ROADMAP.md) for full sprint details.

---

## Quick Start & Usage

### 1. Build from Source
Prerequisites: C++20 compiler (`gcc-12+` or `clang-14+`), CMake 3.22+, and Ninja or Make.

```bash
# Clone the repository
git clone https://github.com/rajpatel/dibyt.git
cd dibyt

# Configure and compile in Release mode
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run the complete test suite
ctest --test-dir build --output-on-failure
```

### 2. Run with Docker Compose
```bash
cd deploy/compose
docker compose up -d
```
- **Control Plane API & Health:** `http://localhost:8080/health`
- **Web Management Dashboard:** `http://localhost:3000`
- **Data Plane (TCP transfer):** `localhost:9080`

### 3. CLI Basic Commands
```bash
# Publish a local directory collection
dibyt publish /path/to/data --name dataset_01 --output manifest.json

# Fetch a collection from a remote Dibyt server
dibyt fetch manifest.json --dest /path/to/downloads --server 127.0.0.1:9080 --workers 4

# Check active session transfer status
dibyt status --session sess_12345 --server 127.0.0.1:8080 --json
```

---

## Benchmark & Performance Profile

Throughput measured via reproducible benchmark harness ([`benchmarks/harness/benchmark_runner.cpp`](benchmarks/harness/benchmark_runner.cpp)):
- **Synthetic Multi-File Collection:** 10 MiB across multiple chunk tasks with 4 parallel worker threads.
- **Measured Throughput:** **124.54 MiB/s** effective rate with real-time SHA-256 calculation and atomic finalization.
- **Target Scale:** 10–100 concurrent peer connections bounded by fixed worker pools.

---

## Documentation Index

- **Product & Architecture:**
  - [Product Requirements (PRD)](docs/product/PRD.md)
  - [Architecture Specification](docs/architecture/ARCHITECTURE.md)
  - [Architecture Decision Records (ADRs)](docs/decisions/01-ARCHITECTURE-DECISIONS.md)
  - [Binary Protocol Specification](docs/protocol/PROTOCOL.md)
  - [REST API OpenAPI Contract](docs/api/openapi.yaml)
- **Deployment & Operations:**
  - [Self-Hosted Deployment Guide](docs/deployment/SELF_HOSTED.md)
  - [Deployment Verification & Smoke Drill](docs/deployment/DEPLOYMENT-VERIFICATION.md)
  - [Production Readiness Checklist](docs/operations/PRODUCTION-READINESS-CHECKLIST.md)
  - [Software Bill of Materials (SBOM)](docs/development/DEPENDENCY_SBOM.md)
- **Security & Quality:**
  - [Final Security Audit Report](docs/security/FINAL-SECURITY-REVIEW.md)
  - [Threat Model](docs/security/THREAT_MODEL.md)
  - [Testing & Quality Standards](docs/development/TESTING.md)
- **Historical Reference:**
  - [Academic Predecessor Prototype Analysis](docs/design/LEGACY_ANALYSIS.md)
  - [Academic Archive Catalog](legacy/academic/cn-project/INDEX.md)

---

## License & Attribution

- Dibyt source code and documentation are released under the **Apache License, Version 2.0**. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
- Historical coursework materials in `legacy/academic/cn-project/` are preserved for academic attribution. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
