# ADR-0001: Modular Monolith Architecture

**Status:** Proposed  
**Date:** 2026-11-06  
**Deciders:** Raj Patel (Project Maintainer)  

---

## Context and Problem Statement
Dibyt requires multiple cooperating subsystems: binary protocol encoding/decoding, network connection management, manifest generation, task scheduling, SQLite persistence, and management APIs. We need an architectural structure that maintains clean subsystem boundaries without incurring excessive operational complexity.

---

## Decision Drivers
- High operational simplicity for self-hosted users (single binary or minimal daemon setup).
- Zero-copy / low-latency data passing between memory buffers and network sockets.
- Strict isolation of responsibilities to allow independent unit testing.
- Prevention of premature operational overhead (e.g., container orchestration, multi-service network RPCs).

---

## Considered Options
1. **Microservices Architecture:** Decomposing into separate daemons for scheduler, storage, transfer workers, and API.
2. **Monolithic Architecture (Single File / Tightly Coupled):** Continuing the academic prototype style where networking, session tracking, and disk I/O reside in a single translation unit.
3. **Modular Monolith (Chosen):** A single deployable binary partitioned into cohesive, decoupled C++ library modules (`libs/core`, `libs/protocol`, `libs/scheduler`, etc.) with strict public interfaces.

---

## Decision Outcome
Chosen option: **Modular Monolith**.

### Positive Consequences
- **Zero Runtime Dependencies:** No external message brokers (RabbitMQ/Kafka) or separate worker services needed.
- **Strong Typings & Fast Interfaces:** Modules pass C++ objects and references across boundaries without JSON/gRPC serialization overhead.
- **Maintainability:** Each module has its own directory, headers, and test suite, preventing spaghetti code.

### Negative Consequences / Trade-offs
- A memory leak or segmentation fault in one module can potentially terminate the entire process if unhandled.
