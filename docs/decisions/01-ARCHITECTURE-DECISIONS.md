# Architecture & Design Decisions — Dibyt

This document consolidates core architectural decision records concerning system decomposition, control vs data plane boundaries, persistence strategies, platform support, and manifest guarantees.

---

## ADR-0001: Modular Monolith Architecture

- **Status:** Proposed
- **Date:** 2026-11-06
- **Deciders:** Raj Patel (Project Maintainer)

### Context & Problem Statement
Dibyt requires multiple cooperating subsystems: binary protocol encoding/decoding, network connection management, manifest generation, task scheduling, SQLite persistence, and management APIs. We need an architectural structure that maintains clean subsystem boundaries without incurring excessive operational complexity.

### Decision Drivers
- High operational simplicity for self-hosted users (single binary or minimal daemon setup).
- Zero-copy / low-latency data passing between memory buffers and network sockets.
- Strict isolation of responsibilities to allow independent unit testing.
- Prevention of premature operational overhead (e.g., container orchestration, multi-service network RPCs).

### Considered Options
1. **Microservices Architecture:** Decomposing into separate daemons for scheduler, storage, transfer workers, and API.
2. **Monolithic Architecture (Single File / Tightly Coupled):** Continuing the academic prototype style where networking, session tracking, and disk I/O reside in a single translation unit.
3. **Modular Monolith (Chosen):** A single deployable binary partitioned into cohesive, decoupled C++ library modules (`libs/core`, `libs/protocol`, `libs/scheduler`, etc.) with strict public interfaces.

### Decision Outcome
Chosen option: **Modular Monolith**.
- **Positive Consequences:** Zero external message brokers or background daemons; strong typings and fast in-process interfaces without serialization overhead; clear modular ownership and isolated tests.
- **Negative Consequences / Trade-offs:** An unhandled fatal violation in one module terminates the process.

---

## ADR-0002: SQLite as Primary Metadata and State Store

- **Status:** Proposed
- **Date:** 2026-11-06
- **Deciders:** Raj Patel (Project Maintainer)

### Context & Problem Statement
Dibyt transfers must be resilient to network disconnections, process crashes, and unexpected host restarts. The legacy academic prototype stored session state in memory on the server and in flat text files on the client. A server crash destroyed all active sessions. We need a persistent storage engine that guarantees ACID transaction semantics without requiring an external database daemon.

### Decision Drivers
- Embedded deployment: no external database server (PostgreSQL/MySQL) setup required by the user.
- ACID durability: transactions must survive sudden power loss.
- High concurrent read performance and transactional write guarantees.
- Lightweight footprint suitable for both embedded edge devices and multi-core servers.

### Considered Options
1. **Plain-Text / JSON Files on Disk:** Storing state files per transfer (similar to legacy client checkpoints).
2. **External Database Daemon (PostgreSQL / MySQL):** Requiring a separate relational database service.
3. **Embedded SQLite (Chosen):** Using SQLite in WAL (Write-Ahead Logging) mode compiled into the binary.

### Decision Outcome
Chosen option: **Embedded SQLite**.
- **Positive Consequences:** Zero user configuration (single `dibyt.db` file); crash recovery via atomic commit and WAL replay; full SQL relational indexing for task leases and durable offsets.
- **Negative Consequences / Trade-offs:** Single active writer transaction at a time; requires concise transaction scopes.

---

## ADR-0003: Control-Plane (HTTP) and Data-Plane (TCP) Separation

- **Status:** Proposed
- **Date:** 2026-11-06
- **Deciders:** Raj Patel (Project Maintainer)

### Context & Problem Statement
Dibyt provides both high-speed bulk data transfer and operational management (session creation, metrics, health status, and web dashboard integration). Routing multi-gigabyte binary file streams through HTTP/JSON REST APIs incurs severe serialization and buffering overhead. Conversely, using raw TCP sockets for RESTful control commands complicates dashboard and orchestration integrations.

### Decision Drivers
- High throughput and zero-copy streaming for large file payloads.
- Easy integration with standard web dashboards, monitoring tools, and orchestrators.
- Clear security boundaries between data plane sockets and administrative management endpoints.

### Considered Options
1. **Unified HTTP/REST for All Traffic:** Transferring files as HTTP multi-part or streaming octet-stream payloads over REST.
2. **Unified Raw TCP for All Traffic:** Implementing custom framing for both binary data and administrative commands.
3. **Dedicated Control Plane (HTTP) & Data Plane (TCP) (Chosen):** Using raw TCP with custom framing for data transfer, and standard HTTP/JSON for management and telemetry.

### Decision Outcome
Chosen option: **Dedicated Control Plane & Data Plane**.
- **Positive Consequences:** Bounded resource consumption; raw TCP zero-copy streaming achieves saturating link speeds; HTTP interface allows standard tooling, Prometheus metrics scrapers, and web consoles to connect without custom binary parsing.
- **Negative Consequences / Trade-offs:** Requires two ports (default 8080 HTTP and 9080 TCP) and clear firewall routing instructions.

---

## ADR-0006: Container Packaging & Self-Hosted Deployment Isolation

- **Status:** Accepted
- **Date:** 2026-11-06
- **Deciders:** Raj Patel (Project Maintainer)

### Context & Problem Statement
Self-hosted users require repeatable, isolated deployments without complicated dependency compilation on host machines. However, file transfer systems need persistent file storage and direct raw TCP ingress that standard container abstractions or layer-7 web PaaS routers do not natively support.

### Decision Drivers
- High operational isolation: containerized daemons must run unprivileged (non-root) and drop all Linux capabilities (`cap_drop: ALL`).
- Data durability: SQLite databases, active checkpoints, and downloaded data must survive container restarts and upgrades.
- Network transparency: TCP binary socket (port 9080) requires layer-4 ingress without intermediate HTTP proxy buffering.

### Decision Outcome
Chosen option: **Multi-stage Docker builds with dedicated persistent volume mounts and drop-privileges security profiles**.
- **Positive Consequences:** Reproducible container builds from Ubuntu 22.04 / Node.js; default unprivileged non-root user `dibyt:dibyt` (UID 10001); read-only configuration mounts (`:ro`); clean systemd service alternative for bare-metal VPS installations.
- **Negative Consequences / Trade-offs:** Requires operators to configure layer-4 firewall rules for port 9080 rather than using simple HTTP reverse proxies alone.

Chosen option: **Dedicated Control Plane (HTTP) & Data Plane (TCP)**.
- **Positive Consequences:** Maximum bulk throughput using raw binary framing and 64 KiB chunks; clean telemetry scraping via Prometheus and standard JSON endpoints; bulk payloads never transit HTTP stacks.
- **Negative Consequences / Trade-offs:** Requires two ports (e.g., 9080 for data plane TCP, 9081 for management HTTP).

---

## ADR-0004: Linux-First Platform Support with Portability Stubs

- **Status:** Proposed
- **Date:** 2026-11-06
- **Deciders:** Raj Patel (Project Maintainer)

### Context & Problem Statement
The predecessor academic project was authored strictly for POSIX/Linux platforms using native Linux socket syscalls (`<sys/socket.h>`, `<netinet/in.h>`, `send(2)`, `recv(2)`). While cross-platform support (Windows/macOS) is valuable, optimizing for multiple OS network stacks simultaneously during early foundation sprints introduces premature complexity.

### Decision Drivers
- High development velocity for foundational sprints.
- Native access to high-performance Linux socket features (`epoll`, `splice`, `fdatasync`).
- Honest representation of tested environments: no false claims of Windows/macOS support without rigorous CI testing.

### Considered Options
1. **Immediate Full Cross-Platform Abstraction (Boost.Asio / libuv):** Introducing heavyweight cross-platform networking frameworks upfront.
2. **Linux-First Native POSIX with Portability Abstraction Layer (Chosen):** Authoring clean socket wrapper interfaces in `libs/networking`, targeting Linux initially, while keeping standard C++20 for all domain, storage, and protocol logic.

### Decision Outcome
Chosen option: **Linux-First Native POSIX with Portability Stubs**.
- **Positive Consequences:** Direct socket control without complex event engine overhead; honest platform boundary documentation for v0.1; C++20 standard library ensures non-socket modules compile anywhere.
- **Negative Consequences / Trade-offs:** Windows native support is deferred to later sprints (requiring Winsock2 abstraction).

---

## ADR-0005: Immutable Manifest Snapshot Policy

- **Status:** Proposed
- **Date:** 2026-11-06
- **Deciders:** Raj Patel (Project Maintainer)

### Context & Problem Statement
When a sender publishes a directory collection, the underlying files on the sender's filesystem could theoretically be modified, appended to, or deleted while an active transfer is underway. If a manifest is dynamic, receivers can experience race conditions, corrupted chunk ranges, and failing checksum verifications.

### Decision Drivers
- Deterministic transfer state across all parallel worker streams.
- Accurate SHA-256 integrity verification.
- Protection against source file mutations during transmission.

### Considered Options
1. **Live Directory Streaming:** Reading file contents dynamically without pre-computing hashes or manifests.
2. **Immutable Snapshot Manifest (Chosen):** Scanning and generating an immutable manifest (with exact file sizes and SHA-256 hashes) at collection creation time. The manifest is versioned and tied to the session.

### Decision Outcome
Chosen option: **Immutable Snapshot Manifest**.
- **Positive Consequences:** Deterministic contract between sender and receivers; immediate detection of source file tampering (`ERR_SOURCE_MUTATED`); clean auditability.
- **Negative Consequences / Trade-offs:** Pre-transfer scan latency to compute initial SHA-256 digests on large collections.
