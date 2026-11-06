# Dibyt 20-Sprint Engineering Roadmap

**Document Status:** Canonical Engineering Roadmap  
**Current Milestone:** Sprint 01 (Repository Audit & Documentation Foundation)

---

## Sprint Index & Execution Status

| Sprint | Code | Title | Core Focus | Status |
|---|---|---|---|---|
| **Sprint 01** | `SPRINT-01` | **Repository Audit & Documentation Foundation** | Repository context, audit of academic prototypes, full docs foundation, ADRs, legal checks. | **Done** |
| **Sprint 02** | `SPRINT-02` | **Repository Structure & Legacy Migration** | Safe relocation of academic codebase into `legacy/academic/cn-project/`, verification of hashes. | Planned |
| **Sprint 03** | `SPRINT-03` | **CMake Foundation & Dependency Policy** | Root and modular `CMakeLists.txt`, presets, dependency strategy, toolchains. | Planned |
| **Sprint 04** | `SPRINT-04` | **Core Domain & Manifest Model** | Relative path sanitization, symlink jail defense, collection scanner, and SHA-256 calculation. | Planned |
| **Sprint 05** | `SPRINT-05` | **Protocol Specification & Frame Codec** | Fixed 16-byte binary frame codec, magic bytes, length limits, and serialization. | Planned |
| **Sprint 06** | `SPRINT-06` | **TCP Transport & Connection Lifecycle** | RAII socket management, bounded I/O loops, timeout handling, non-blocking polling. | Planned |
| **Sprint 07** | `SPRINT-07` | **SHA-256 Integrity & Safe Finalization** | Staging quarantine directory, streaming OpenSSL EVP digests, atomic rename finalization. | Planned |
| **Sprint 08** | `SPRINT-08` | **SQLite Persistence & Schema Migrations** | Embedded SQLite schema, migrations engine, transaction boundaries, session persistence. | Planned |
| **Sprint 09** | `SPRINT-09` | **Durable Checkpoint & Resume Engine** | Flush-before-ACK invariant, durable byte progression, random-access resume seek. | Planned |
| **Sprint 10** | `SPRINT-10` | **Dynamic Scheduler & Bounded Worker Pool** | Leased task queue, work-stealing, heartbeat monitoring, and lease timeouts. | Planned |
| **Sprint 11** | `SPRINT-11` | **End-to-End Parallel TCP Transfer** | Multi-stream parallel file transfers, dynamic worker load balancing. | Planned |
| **Sprint 12** | `SPRINT-12` | **Fault Tolerance, Retry & Restart Recovery** | Chaos testing, connection failure injection, process crash recovery, bounded retry backoff. | Planned |
| **Sprint 13** | `SPRINT-13` | **CLI Product Experience** | Production CLI with CLI11, progress bars, structured output, clean exit codes. | Planned |
| **Sprint 14** | `SPRINT-14` | **Configuration, Logging, Metrics & Health** | Structured JSON logging, Prometheus metrics exporter, health checks, runtime configuration. | Planned |
| **Sprint 15** | `SPRINT-15` | **Security Foundations: Auth, TLS & FS Jails** | Mutual TLS encryption, session tokens, canonical filesystem sandboxing. | Planned |
| **Sprint 16** | `SPRINT-16` | **Versioned Management REST API** | Lightweight C++ HTTP REST API for control-plane status and lifecycle management. | Planned |
| **Sprint 17** | `SPRINT-17` | **React/TypeScript Management Dashboard** | Clean web UI for monitoring active transfers, worker health, and historical sessions. | Planned |
| **Sprint 18** | `SPRINT-18` | **Comprehensive Testing, Chaos & Benchmarks** | Rigorous benchmarks across 10–100 clients, network emulation (`tc`), stress tests. | Planned |
| **Sprint 19** | `SPRINT-19` | **Docker Packaging, CI/CD & Self-Hosting** | Multi-stage Docker builds, Docker Compose, systemd unit definitions, GitHub Actions. | Planned |
| **Sprint 20** | `SPRINT-20` | **Final Cleanup, Security & Production Readiness** | Final security audit, static analysis, sanitizers, documentation review, v0.1 release tag. | Planned |
