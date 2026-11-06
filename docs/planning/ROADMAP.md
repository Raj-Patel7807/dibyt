# Dibyt 20-Sprint Engineering Roadmap

**Document Status:** Canonical Engineering Roadmap  
**Current Milestone:** Sprint 20 Completed (All 20 Sprints Implemented & Verified)  
**Release Target:** v0.1.0-dev (`developer preview`)  

---

## Sprint Index & Execution Status

| Sprint | Code | Title | Core Focus | Status |
|---|---|---|---|---|
| **Sprint 01** | `SPRINT-01` | **Repository Audit & Documentation Foundation** | Repository context, audit of academic prototypes, full docs foundation, ADRs, legal checks. | **Done** |
| **Sprint 02** | `SPRINT-02` | **Repository Structure & Legacy Migration** | Safe relocation of academic codebase into `legacy/academic/cn-project/`, verification of hashes. | **Done** |
| **Sprint 03** | `SPRINT-03` | **CMake Foundation & Dependency Policy** | Root and modular `CMakeLists.txt`, presets, dependency strategy, toolchains. | **Done** |
| **Sprint 04** | `SPRINT-04` | **Core Domain & Manifest Model** | Relative path sanitization, symlink jail defense, collection scanner, and SHA-256 calculation. | **Done** |
| **Sprint 05** | `SPRINT-05` | **Protocol Specification & Frame Codec** | Fixed 32-byte binary frame codec, magic bytes, length limits, CRC32, and serialization. | **Done** |
| **Sprint 06** | `SPRINT-06` | **TCP Transport & Connection Lifecycle** | RAII socket management, bounded I/O loops, timeout handling, non-blocking polling. | **Done** |
| **Sprint 07** | `SPRINT-07` | **SHA-256 Integrity & Safe Finalization** | Staging quarantine directory, streaming SHA-256 digests, atomic rename finalization. | **Done** |
| **Sprint 08** | `SPRINT-08` | **SQLite Persistence & Schema Migrations** | Embedded SQLite schema, migrations engine, transaction boundaries, session persistence. | **Done** |
| **Sprint 09** | `SPRINT-09` | **Durable Checkpoint & Resume Engine** | Flush-before-ACK invariant, durable byte progression, random-access resume seek. | **Done** |
| **Sprint 10** | `SPRINT-10` | **Dynamic Scheduler & Bounded Worker Pool** | Leased task queue, dynamic LPT scheduling, fence generations, and lease timeouts. | **Done** |
| **Sprint 11** | `SPRINT-11` | **End-to-End Parallel TCP Transfer** | Multi-stream parallel file transfers, dynamic worker load balancing. | **Done** |
| **Sprint 12** | `SPRINT-12` | **Fault Tolerance, Retry & Restart Recovery** | Chaos testing, connection failure injection, process crash recovery, bounded retry backoff. | **Done** |
| **Sprint 13** | `SPRINT-13` | **CLI Product Experience** | Production CLI with subcommands (`publish`, `fetch`, `status`), JSON output, clean exit codes. | **Done** |
| **Sprint 14** | `SPRINT-14` | **Configuration, Logging, Metrics & Health** | Structured JSON logging, Prometheus metrics exporter, health checks, runtime configuration. | **Done** |
| **Sprint 15** | `SPRINT-15` | **Security Foundations: Auth, TLS & FS Jails** | Constant-time token authentication, default-deny policy, canonical filesystem sandboxing. | **Done** |
| **Sprint 16** | `SPRINT-16` | **Versioned Management REST API** | Lightweight C++ HTTP REST API router (`/api/v1`), OpenAPI specification. | **Done** |
| **Sprint 17** | `SPRINT-17` | **React/TypeScript Management Dashboard** | Clean React 19 + TypeScript web UI for monitoring active transfers, collections, and health. | **Done** |
| **Sprint 18** | `SPRINT-18` | **Comprehensive Testing, Chaos & Benchmarks** | Fault-injection suite, chaos simulations, and reproducible throughput benchmark harness. | **Done** |
| **Sprint 19** | `SPRINT-19` | **Docker Packaging, CI/CD & Self-Hosting** | Multi-stage Docker builds, Docker Compose, systemd unit definitions, hardened CI/CD pipeline. | **Done** |
| **Sprint 20** | `SPRINT-20` | **Final Cleanup, Security & Production Readiness** | Final security audit, dependency SBOM, deployment verification drill, release gate checklist. | **Done** |
