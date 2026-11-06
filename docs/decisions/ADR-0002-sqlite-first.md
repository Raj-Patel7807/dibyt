# ADR-0002: SQLite as Primary Metadata and State Store

**Status:** Proposed  
**Date:** 2026-11-06  
**Deciders:** Raj Patel (Project Maintainer)  

---

## Context and Problem Statement
Dibyt transfers must be resilient to network disconnections, process crashes, and unexpected host restarts. The legacy academic prototype stored session state in memory on the server and in flat text files on the client. A server crash destroyed all active sessions. We need a persistent storage engine that guarantees ACID transaction semantics without requiring an external database daemon.

---

## Decision Drivers
- Embedded deployment: no external database server (PostgreSQL/MySQL) setup required by the user.
- ACID durability: transactions must survive sudden power loss.
- High concurrent read performance and transactional write guarantees.
- Lightweight footprint suitable for both embedded edge devices and multi-core servers.

---

## Considered Options
1. **Plain-Text / JSON Files on Disk:** Storing state files per transfer (similar to legacy client checkpoints).
2. **External Database Daemon (PostgreSQL / MySQL):** Requiring a separate relational database service.
3. **Embedded SQLite (Chosen):** Using SQLite in WAL (Write-Ahead Logging) mode compiled into the binary.

---

## Decision Outcome
Chosen option: **Embedded SQLite**.

### Positive Consequences
- **Zero Configuration:** Database resides in a single file (`dibyt.db`) created automatically on first run.
- **Crash Recovery:** SQLite's atomic commit and WAL mode ensure database integrity even during abrupt power loss or SIGKILL.
- **Relational Query Power:** Enables fast indexing and filtering over transfer tasks, worker leases, and durable offsets.

### Negative Consequences / Trade-offs
- Concurrency limit: SQLite allows many concurrent readers but only one active writer transaction at a time. Concurrency must be managed carefully using short transaction windows.
