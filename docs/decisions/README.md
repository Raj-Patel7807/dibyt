# Architecture Decision Records (ADRs) — Dibyt

This directory documents the significant architectural and technical decisions made during the design and evolution of **Dibyt**.

Rather than spreading each individual decision across dozens of isolated single-record files, decisions are consolidated into cohesive domain decision documents:

---

## Consolidated Decision Files

| File | Scope & Contained ADRs | Status |
|---|---|---|
| [**`01-ARCHITECTURE-DECISIONS.md`**](01-ARCHITECTURE-DECISIONS.md) | **System Architecture & Core Technical Choices**<br>• `ADR-0001`: Modular Monolith Architecture<br>• `ADR-0002`: SQLite as Primary Metadata and State Store<br>• `ADR-0003`: Control-Plane (HTTP) and Data-Plane (TCP) Separation<br>• `ADR-0004`: Linux-First Platform Support with Portability Stubs<br>• `ADR-0005`: Immutable Manifest Snapshot Policy | **Proposed** |
| [**`02-PROTOCOL-AND-NETWORKING-DECISIONS.md`**](02-PROTOCOL-AND-NETWORKING-DECISIONS.md) | Framing, transport loops, timeout policy, backpressure, and TLS (Sprints 05–06, 15). | *Reserved for upcoming sprints* |
| [**`03-DATA-INTEGRITY-AND-PERSISTENCE-DECISIONS.md`**](03-DATA-INTEGRITY-AND-PERSISTENCE-DECISIONS.md) | Checkpoint flush invariants, schema migration strategies, and staging files (Sprints 07–09). | *Reserved for upcoming sprints* |
| [**`04-OPERATIONS-AND-PACKAGING-DECISIONS.md`**](04-OPERATIONS-AND-PACKAGING-DECISIONS.md) | CLI UX, containerization, and configuration discovery (Sprints 13–14, 19). | *Reserved for upcoming sprints* |

---

## Authoring Guidelines

When proposing new decisions:
- Consult [`ADR-TEMPLATE.md`](ADR-TEMPLATE.md) for standard fields.
- Append new records to the relevant categorized decision file rather than creating one-off markdown files.
