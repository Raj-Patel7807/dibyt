# Architecture Decision Records (ADRs)

This directory documents the significant architectural decisions made during the evolution of **Dibyt**.

We use Architecture Decision Records to capture context, alternatives evaluated, rationale, and consequences for key technical choices.

---

## Decision Record Index

| ADR | Title | Status | Date |
|---|---|---|---|
| [ADR-0001](ADR-0001-modular-monolith.md) | Modular Monolith Architecture | **Proposed** | 2026-11-06 |
| [ADR-0002](ADR-0002-sqlite-first.md) | SQLite as Primary Metadata and State Store | **Proposed** | 2026-11-06 |
| [ADR-0003](ADR-0003-tcp-data-plane-http-control-plane.md) | Control-Plane (HTTP) and Data-Plane (TCP) Separation | **Proposed** | 2026-11-06 |
| [ADR-0004](ADR-0004-linux-first-platform-support.md) | Linux-First Platform Support with Portability Stubs | **Proposed** | 2026-11-06 |
| [ADR-0005](ADR-0005-immutable-manifest-snapshots.md) | Immutable Manifest Snapshot Policy | **Proposed** | 2026-11-06 |

---

## Guidelines for Authoring New ADRs

When proposing a new architectural decision, copy [`ADR-TEMPLATE.md`](ADR-TEMPLATE.md) and assign the next sequential number. All ADRs remain `Proposed` until reviewed and formally approved by project maintainers.
